/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "./allocator_interface.h"
#include "./allocator_helper.h"
#include "./my_checker.h"
#include "./memlib.h"

// Don't call libc malloc!
#define malloc(...) (USE_MY_MALLOC)
#define free(...) (USE_MY_FREE)
#define realloc(...) (USE_MY_REALLOC)

// All blocks must have a specified minimum alignment.
// The alignment requirement (from config.h) is >= 8 bytes.
#ifndef ALIGNMENT
#define ALIGNMENT 8
#endif

// Rounds up to the nearest multiple of ALIGNMENT.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define IS_ALIGNED(ptr) ((((uint64_t) ptr) & (ALIGNMENT-1)) == 0)

// This is a link to all the bins that contain free chunks of memory.
// They are of the following sizes:
// 0. (Special) Points to the end of the heap.
// 1. (Special) A chunk that will be used/split if a malloc can't find space
// 2. (Special) HUUUUGE CHUNKS (> HUGE_BIN_CUTOFF)
// 3. 24 bytes
// 4. 32 bytes
// ...
// 31. 248 bytes
// 32. 256 - 383 bytes
// 33. 384 - 511 bytes
// 34. 512 - 767 bytes
// ...
// 62. 8388608 - 12582911 bytes
// 63. 12582912 - 16777215 bytes
#define NUM_OF_BINS 64
static chunk_t* bins[NUM_OF_BINS];

typedef unsigned int bin_index;

#define LARGE_CHUNK_CUTOFF 249
// Anything 249 bytes or larger will go in the "large" bins

#define IS_LARGE_CHUNK(chunk_ptr) (CHUNK_SIZE(chunk_ptr) > LARGE_CHUNK_CUTOFF)
#define IS_LARGE_SIZE(size) ((size) > LARGE_CHUNK_CUTOFF)

#define IS_SMALL_CHUNK(chunk_ptr) (!IS_LARGE_CHUNK(chunk_ptr))
#define IS_SMALL_SIZE(size) (!IS_LARGE_SIZE(size))

#define HUGE_CHUNK_CUTOFF 16777215
// Anything larger than this goes in the huge bin

#define IS_HUGE_CHUNK(chunk_ptr) (CHUNK_SIZE(chunk_ptr) > HUGE_CHUNK_CUTOFF)
#define IS_HUGE_SIZE(size) ((size) > HUGE_CHUNK_CUTOFF)

#define END_OF_HEAP_BIN (bins[0])
#define VICTIM_BIN (bins[1])
#define HUGE_BIN (bins[2])


/* ------------------------------------------------------------------------- */
// [START STATIC METHOD DECLARATIONS]
// Below this is just all the static methods declared so they can be used anywhere in this file

static int remove_large_chunk(bigchunk_t* chunk);
static int remove_small_chunk(chunk_t* chunk);
static int remove_chunk(chunk_t* chunk);
static int insert_large_chunk(bigchunk_t* chunk);
static int insert_small_chunk(chunk_t* chunk);
static int insert_chunk(chunk_t* chunk);
static chunk_t* small_malloc(size_int request);
static chunk_t* large_malloc(size_int request);
static chunk_t* end_of_heap_malloc(size_int request);
static chunk_t* combine_chunks(chunk_t* left, chunk_t* right);


// [END STATIC METHOD DECLARATIONS]
/* ------------------------------------------------------------------------- */

int my_check() {
  return my_checker(bins, NUM_OF_BINS);
}

// init - Initialize the malloc package.  Called once before any other
// calls are made.

// This simple implementation initializes some large chunk of memory right off the
// bat by sbrk

#ifndef INITIAL_CHUNK_SIZE
#define INITIAL_CHUNK_SIZE (8192)
#endif

int my_init() {
  void *brk = mem_heap_hi() + 1;
  int req_size = ALIGN((uint64_t)brk) - (uint64_t)brk;
  if (req_size != 0)
    mem_sbrk(req_size);
  assert(IS_ALIGNED(mem_heap_hi() + 1));
  chunk_t* first_chunk = mem_sbrk(INITIAL_CHUNK_SIZE + 2*sizeof(size_int));
  assert(IS_ALIGNED(first_chunk));
  first_chunk->current_size = INITIAL_CHUNK_SIZE;
  SET_PREVIOUS_INUSE(first_chunk);
  END_OF_HEAP_BIN = first_chunk;
  assert(IS_END_OF_HEAP(first_chunk));
  assert(IS_END_OF_HEAP(END_OF_HEAP_BIN));
  #ifdef DEBUG
  assert(my_check() == 0);
  #endif
  return 0;
}

/* ------------------------------------------------------------------------- */
// [START INDEXING METHODS]

static inline bin_index small_request_index(size_int request) {
  assert(IS_SMALL_SIZE(request));
  bin_index result = request >> 3;
  assert(result > 2 && result < 33);
  return result;
}


static inline bin_index large_request_index(size_int request) {
  assert(IS_LARGE_SIZE(request));
  int l = FAST_LOG2(request);
  bin_index result = 16 + 2*l + ((request >> (l-1)) & 1);
  assert(result > 31 && result < 64);
  return result;
}
// [END INDEXING METHODS]
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
// [START CHUNK INSERT/REMOVE METHODS]
// Below lies the methods to insert and remove chunks from their respective bins
// There are different methods for large and small chunks

// Pseudocode - If this chunk is in a circularly linked list of length > 1
// unlink the node from the linked list and if this node has a parent, set the
// parent of the next node to the parent of this node.

// If this chunk is by itself in it's linked list, traverse down the tree (perhaps
// by going left, and then right as much as possible) until you find a node with
// no children. Promote this childless node (or linked list) to the top by copying
// all the parameters from the old root to this new root.

// Finally, if the parent isn't NO_PARENT_ROOT_NODE, set the corresponding
// left or right child to the new child node. Return 0. If the parent is NO_PARENT_ROOT_NODE,
// then point the bucket to the right spot. Return 1.
static int remove_large_chunk(bigchunk_t* chunk) {
  assert(IS_LARGE_CHUNK(chunk));
  if (IS_HUGE_CHUNK(chunk)) {
    assert(1 == 0);
    // TODO
    printf("remove: huge chunk not done yet...\n");
    exit(1);
  }
  size_int size = CHUNK_SIZE(chunk);
  bin_index n = large_request_index(size);
  bigchunk_t* replacement;
  if (CIRCULAR_LIST_IS_LENGTH_ONE(chunk)) {
    bigchunk_t* current;
    // First, go down the left side
    current = chunk->children[0];
    // "If it has at least one child, go to that child"
    while (current != NULL && (current->children[0] != NULL || current->children[1] != NULL)) {
      current = (current->children[1] != NULL) ? current->children[1] : current->children[0];
    }
    // If we didn't find anything, go down right side
    if (current == NULL) {
      current = chunk->children[1];
      while (current != NULL && (current->children[0] != NULL || current->children[1] != NULL)) {
        current = (current->children[0] != NULL) ? current->children[0] : current->children[1];
      }
    }
    replacement = current;
  } else {
    chunk->prev->next = chunk->next;
    chunk->next->prev = chunk->prev;
    replacement = chunk->next;
  }
  if (replacement != NULL) {
    replacement->parent = chunk->parent;
    replacement->children[0] = chunk->children[0];
    replacement->children[1] = chunk->children[1];
    replacement->shift = chunk->shift;
  }
  if (chunk->parent == NO_PARENT_ROOT_NODE) {
    bins[n] = (chunk_t*) replacement;
  } else if (chunk->parent != NULL) {
    chunk->parent->children[(size >> chunk->parent->shift) & 1] = replacement;
  }
  chunk->next = chunk->prev = NULL;
  return 0;
}

// Pseudocode - If the linked list size is 1, set the bin's pointer to NULL and call
// it a day RETURN 1 to let callers know it was the last one. Otherwise, if this
// chunk is the bin's pointer, set the bin's pointer to the next chunk unlink this
// node and return 0.
static int remove_small_chunk(chunk_t* chunk) {
  assert(IS_SMALL_CHUNK(chunk));
  bin_index n = small_request_index(CHUNK_SIZE(chunk));
  if (CIRCULAR_LIST_IS_LENGTH_ONE(chunk)) {
    bins[n] = NULL;
    return 1;
  }
  if (bins[n] == chunk)
    bins[n] = chunk->next;
  chunk->next->prev = chunk->prev;
  chunk->prev->next = chunk->next;
  chunk->next = chunk->prev = NULL;
  return 0;
}

static int remove_chunk(chunk_t* chunk) {
  if (IS_LARGE_CHUNK(chunk))
    return remove_large_chunk((bigchunk_t*) chunk);
  else
    return remove_small_chunk(chunk);
}

// Pseudocode - First, calculate the bin this node should go in. Set the bin
// field of this chunk to that. If that bin is empty:
// Calculate the fast log of the size, and subtract two. This is the "shift" field
// Set the parent to NO_PARENT_ROOT_NODE and set the bin to point to this node. Return 2.
// If that bin has elements: Traverse down the tree by going to the child that
// corresponds to (chunk_size >> shift) & 1. Stop when one of these things happens:
// 1. The node you're currently at is CHUNK_SIZES_EQUAL to the other chunk. Insert
// This node behind the node you're currently looking at in the linked list and
// update all the correct fields. Return 0.
// 2. You're about to go down a path where the child is null. Simply set the parent
// of this node to the parent, and the child of the parent to you. Set the left and
// right pointers of the linked list to yourself. Return 1.
// As you're traversing down the list, the "shift" field should be 1 less than it's parent.
static int insert_large_chunk(bigchunk_t* chunk) {
  assert(IS_LARGE_CHUNK(chunk));
  if (IS_HUGE_CHUNK(chunk)) {
    assert(1 == 0);
    // TODO
    printf("insert: huge chunk not done yet...\n");
    exit(1);
  }
  bin_index n = large_request_index(CHUNK_SIZE(chunk));
  chunk->bin_number = n;
  chunk->children[0] = NULL;
  chunk->children[1] = NULL;
  bigchunk_t* parent = NO_PARENT_ROOT_NODE;
  bigchunk_t* current = (bigchunk_t*) bins[n];
  size_int size = CHUNK_SIZE(chunk);
  while (current != NULL) {
    if (CHUNK_SIZES_EQUAL(current, chunk)) {
      chunk->parent = NO_PARENT_CIRCLE_NODE;
      chunk->shift = current->shift;
      chunk->next = current;
      chunk->prev = current->prev;
      current->prev->next = chunk;
      current->prev = chunk;
      return 0;
    }
    parent = current;
    current = current->children[(size >> current->shift) & 1];
  }
  assert(current == NULL);
  chunk->parent = parent;
  int result = 0;
  if (parent == NO_PARENT_ROOT_NODE) {
    // This means it skipped the while loop.
    chunk->shift = FAST_LOG2(CHUNK_SIZE(chunk)) - 2;
    bins[n] = (chunk_t*) chunk;
    result = 2;
  } else {
    parent->children[(size >> parent->shift) & 1] = chunk;
    chunk->shift = parent->shift - 1;
    result = 1;
  }
  chunk->next = chunk;
  chunk->prev = chunk;
  return result;
}

// Pseudocode - find the bin this chunk corresponds to. If that bin is empty, set the left
// and right pointers to itself, return 1 at the end.
// If the bin is not empty, insert this chunk before the bin pointer's chunk. return 0 at the end
// Finally, set the bin pointer to this chunk and return the correct value.
static int insert_small_chunk(chunk_t* chunk) {
  assert(IS_SMALL_CHUNK(chunk));
  int result = 0;
  bin_index n = small_request_index(CHUNK_SIZE(chunk));
  if (bins[n] == NULL) {
    chunk->next = chunk;
    chunk->prev = chunk;
    result = 1;
  } else {
    chunk->next = bins[n];
    chunk->prev = bins[n]->prev;
    chunk->prev->next = chunk;
    chunk->next->prev = chunk;
  }
  bins[n] = chunk;
  return result;
}

static int insert_chunk(chunk_t* chunk) {
  if (IS_LARGE_CHUNK(chunk))
    return insert_large_chunk((bigchunk_t*) chunk);
  else
    return insert_small_chunk(chunk);
}

// [END CHUNK INSERT/REMOVE METHODS]
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
// [BEGIN MALLOC METHODS]


#define CAN_SPLIT_CHUNK(chunk_ptr, size) (CHUNK_SIZE(chunk_ptr) >= size + SMALLEST_CHUNK)

// Splits a chunk in two pieces, and returns the address of the second chunk
static chunk_t* split_chunk(chunk_t* chunk, size_int request) {
  assert(CAN_SPLIT_CHUNK(chunk, request));
  assert(IS_CURRENT_FREE(chunk));
  size_int leftover = CHUNK_SIZE(chunk) - request - sizeof(size_int);
  chunk->current_size = request | IS_PREVIOUS_INUSE(chunk);
  chunk_t* next_chunk = NEXT_HEAP_CHUNK(chunk);
  next_chunk->previous_size = request;
  next_chunk->current_size = leftover;
  if (!IS_END_OF_HEAP(next_chunk))
    NEXT_HEAP_CHUNK(next_chunk)->previous_size = leftover;
  return next_chunk;
}

// Pseudocode - First, go to the index that would service the request. If there is a free chunk there,
// unlink it and remove it. If that don't work, go to the next bin, and if that doesn't work, see if
// the victim chunk is large enough. Otherwise return NULL
static chunk_t* small_malloc(size_int request) {
  bin_index i = small_request_index(request);
  chunk_t* result = bins[i];
  if (result != NULL) {
    remove_small_chunk(result);
    return result;
  }
  // First bin didn't work? Try next bin
  if (i < 31) {
    result = bins[i+1];
    if (result != NULL) {
      remove_small_chunk(result);
      return result;
    }
  }
  // First two bins didn't work? Try the victim
  // Below can be consolidated into a single if statement.
  if (VICTIM_BIN != NULL && // If it exists
      CAN_SPLIT_CHUNK(VICTIM_BIN, request)) /* And is large enough to be split */ {
    result = VICTIM_BIN;
    VICTIM_BIN = split_chunk(VICTIM_BIN, request);
  } else if (VICTIM_BIN != NULL && CHUNK_SIZE(VICTIM_BIN) >= request) {
    result = VICTIM_BIN;
    VICTIM_BIN = NULL;
  }
  return result;
}


// Pseudocode - First go to the index that would service the request. If there no free chunk that works,
// take the smallest chunk from the next bin (optional?). If either of these work, split it and set the remainder to
// the victim chunk (and add the victim chunk if it exists to a bin). Otherwise return NULL.
static chunk_t* large_malloc(size_int request) {
  if (IS_HUGE_SIZE(request)) {
    assert(1 == 0);
    // TODO
    printf("malloc: huge chunk not done yet...\n");
    exit(1);
  }
  bin_index n = large_request_index(request);
  bigchunk_t* best_chunk = NULL;
  size_int best_size = 2*request; // Guaranteted to not be in this bin.
  bigchunk_t* current = (bigchunk_t*) bins[n];
  while (current != NULL) {
    int decision = (request >> current->shift) & 1;
    if (CHUNK_SIZE(current) == request) {
      best_chunk = current;
      break;
    } else if (CHUNK_SIZE(current) < best_size && CHUNK_SIZE(current) > request) {
      best_size = CHUNK_SIZE(current);
      best_chunk = current;
    }
    if (!decision && current->children[1] != NULL && CHUNK_SIZE(current->children[1]) < best_size) {
      best_size = CHUNK_SIZE(current->children[1]);
      best_chunk = current->children[1];
    }
    current = current->children[decision];
  }
  if (best_chunk == NULL && n < 63 && bins[n+1] != NULL) {
    // Find smallest chunk in next bin;
    best_chunk = (bigchunk_t*) bins[n+1];
    best_size = CHUNK_SIZE(bins[n+1]);
    while (current != NULL) {
      if (CHUNK_SIZE(current) < best_size) {
        best_chunk = current;
        best_size = CHUNK_SIZE(current);
      }
      current = (current->children[0] != NULL) ? current->children[0] : current->children[1];
    }
  }
  if (best_chunk == NULL)
    return NULL;
  remove_large_chunk(best_chunk);
  if (CAN_SPLIT_CHUNK(best_chunk, request)) {
    if (VICTIM_BIN != NULL)
      insert_chunk(VICTIM_BIN);
    VICTIM_BIN = split_chunk((chunk_t*) best_chunk, request);
  }
  return (chunk_t*) best_chunk;
}


#ifndef EXTENSION_SIZE
#define EXTENSION_SIZE (SMALLEST_CHUNK + 128)
#endif

// Pseudocode - Extend the last chunk as far as needed so it can be split into two chunks
// If it can't be extended that far, return null, otherwise split it in two.
static chunk_t* end_of_heap_malloc(size_int request) {
  if (!CAN_SPLIT_CHUNK(END_OF_HEAP_BIN, request)) {
    void* new = mem_sbrk(request - CHUNK_SIZE(END_OF_HEAP_BIN) + EXTENSION_SIZE);
    END_OF_HEAP_BIN->current_size += request - CHUNK_SIZE(END_OF_HEAP_BIN) + EXTENSION_SIZE;
    if (new == NULL)
      return NULL;
  }
  chunk_t* result = END_OF_HEAP_BIN;
  END_OF_HEAP_BIN = split_chunk(END_OF_HEAP_BIN, request);
  return result;
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.

// Pseudocode - to malloc a block, first round up the requested size to the nearest
// multiple of alignment, or SMALLEST_MALLOC, whichever is larger. If this requested size is smaller
// than the threshold, malloc a small chunk. If this requested size is larger than the
// threshold, malloc a large chunk.

#define MAX(a, b) ((a) ^ (((a) ^ (b)) & -((a) < (b))))

void * my_malloc(size_t size) {
  #ifdef VERBOSE
  printf("============================ Malloc %lld ============================\n", size);
  #endif
  if (size == 0)
    return NULL;
  size_int aligned_size = ALIGN(size);
  size_int request = MAX(aligned_size, SMALLEST_MALLOC);
  chunk_t* result = NULL;
  if (IS_LARGE_SIZE(request))
    result = large_malloc(request);
  else
    result = small_malloc(request);
  if (result == NULL)
    result = end_of_heap_malloc(request);
  if (result != NULL) {
    SET_CURRENT_INUSE(result);
    SET_PREVIOUS_INUSE(NEXT_HEAP_CHUNK(result));
  }
  #ifdef DEBUG
  assert(my_check() == 0);
  #endif
  return CHUNK_TO_USER_POINTER(result);
}
// [END MALLOC METHODS]
/* ------------------------------------------------------------------------- */

#define COMBINED_SIZES(chunk_ptr_left, chunk_ptr_right) \
    (SAFE_SIZE((chunk_ptr_left)->current_size) + SAFE_SIZE((chunk_ptr_right)->current_size) + sizeof(size_int))
#define CAN_COMBINE_PREVIOUS(chunk_ptr) (IS_PREVIOUS_FREE(chunk_ptr))
#define CAN_COMBINE_NEXT(chunk_ptr) (!IS_END_OF_HEAP(chunk_ptr) && IS_CURRENT_FREE(NEXT_HEAP_CHUNK(chunk_ptr)))


// Combines two chunks together to form a single larger chunk. Assumes
// the two chunks can be combined.
static chunk_t* combine_chunks(chunk_t* left, chunk_t* right) {
  size_int combined = COMBINED_SIZES(left, right);
  if (!IS_END_OF_HEAP(right)) {
    NEXT_HEAP_CHUNK(right)->previous_size = combined;
    // "Free" bit should already be set, otherwise we wouldn't be combining.
    assert(IS_PREVIOUS_FREE(NEXT_HEAP_CHUNK(right)));
  }
  if (VICTIM_BIN == left || VICTIM_BIN == right)
    VICTIM_BIN = NULL;
  left->current_size = combined | IS_PREVIOUS_INUSE(left);
  assert(IS_CURRENT_FREE(left));
  return left;
}

/*
Pseudocode - to free a ptr, first check if the previous and next chunks are free.

DON'T CHECK THE NEXT CHUNK IF THE CURRENT CHUNK IS THE LAST CHUNK.

If they are, remove them from their bins so that we can combine with them safely.

If either the front or back pointers were the designated victim, set the victim bin to NULL

Then, combine with them so we get one large chunk.

If this chunk (possibly combined now) is at the end of the heap, replace the END_OF_HEAP_BIN
with this chunk, and call it a day.

Otherwise, insert this new chunk into the corresponding bin.

Finally, clear the PREVIOUS_INUSE bit of the next chunk, and write the previous_size of the next chunk.
*/

void my_free(void *ptr) {
  #ifdef VERBOSE
  printf("============================ Free ============================\n");
  #endif
  chunk_t* chunk = USER_POINTER_TO_CHUNK(ptr);
  CLEAR_CURRENT_INUSE(chunk);
  if (CAN_COMBINE_PREVIOUS(chunk)) { // This order is important, since the next chunk has to have the size at the end;
    chunk_t* prev_chunk = PREVIOUS_HEAP_CHUNK(chunk);
    remove_chunk(prev_chunk);
    chunk = combine_chunks(prev_chunk, chunk);
  }
  if (CAN_COMBINE_NEXT(chunk)) {
    chunk_t* next_chunk = NEXT_HEAP_CHUNK(chunk);
    if (!IS_END_OF_HEAP(next_chunk))
      remove_chunk(next_chunk);
    chunk = combine_chunks(chunk, next_chunk);
  } else {
    CLEAR_PREVIOUS_INUSE(NEXT_HEAP_CHUNK(chunk));
  }
  if (IS_END_OF_HEAP(chunk)) {
    END_OF_HEAP_BIN = chunk;
  } else {
    insert_chunk(chunk);
  }
  #ifdef DEBUG
  assert(my_check() == 0);
  #endif
}

// realloc - Implemented simply in terms of malloc and free
void * my_realloc(void *ptr, size_t size) {
  void *newptr;
  size_t copy_size;

  // Allocate a new chunk of memory, and fail if that allocation fails.
  newptr = my_malloc(size);
  if (NULL == newptr)
    return NULL;

  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  copy_size = SAFE_SIZE(USER_POINTER_TO_CHUNK(ptr)->current_size);

  // If the new block is smaller than the old one, we have to stop copying
  // early so that we don't write off the end of the new block of memory.
  if (size < copy_size)
    copy_size = size;

  // This is a standard library call that performs a simple memory copy.
  memcpy(newptr, ptr, copy_size);

  // Release the old block.
  my_free(ptr);

  #ifdef DEBUG
  assert(my_check() == 0);
  #endif
  // Return a pointer to the new block.
  return newptr;
}

// call mem_reset_brk.
void my_reset_brk() {
  mem_reset_brk();
}

// call mem_heap_lo
void * my_heap_lo() {
  return mem_heap_lo();
}

// call mem_heap_hi
void * my_heap_hi() {
  return mem_heap_hi();
}
