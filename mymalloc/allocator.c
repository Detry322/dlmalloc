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

#define LARGE_CHUNK_CUTOFF 249
// Anything 249 bytes or larger will go in the "large" bins

#define IS_LARGE_CHUNK(chunk_ptr) (SAFE_SIZE((chunk_ptr)->current_size) > LARGE_CHUNK_CUTOFF)
#define IS_LARGE_SIZE(size) ((size) > LARGE_CHUNK_CUTOFF)

#define HUGE_CHUNK_CUTOFF 16777215
// Anything larger than this goes in the huge bin

#define IS_HUGE_CHUNK(chunk_ptr) (SAFE_SIZE((chunk_ptr)->current_size) > HUGE_CHUNK_CUTOFF)
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
static void* small_malloc(size_int request);
static void* large_malloc(size_int request);

// [END STATIC METHOD DECLARATIONS]
/* ------------------------------------------------------------------------- */


// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.

int my_check() {
  return my_checker(bins, NUM_OF_BINS);
}

/* ------------------------------------------------------------------------- */
// [START CHUNK INSERT/REMOVE METHODS]
// Below lies the methods to insert and remove chunks from their respective bins
// There are different methods for large and small chunks

static int remove_large_chunk(bigchunk_t* chunk) {
  // TODO
}

static int remove_small_chunk(chunk_t* chunk) {

}

static int remove_chunk(chunk_t* chunk) {
  if (IS_LARGE_CHUNK(chunk))
    return remove_large_chunk((bigchunk_t*) chunk);
  else
    return remove_small_chunk(chunk);
}

static int insert_large_chunk(bigchunk_t* chunk) {

}

static int insert_small_chunk(chunk_t* chunk) {

}

static int insert_chunk(chunk_t* chunk) {
  if (IS_LARGE_CHUNK(chunk))
    return insert_large_chunk((bigchunk_t*) chunk);
  else
    return insert_small_chunk(chunk);
}

// [END CHUNK INSERT/REMOVE METHODS]
/* ------------------------------------------------------------------------- */


// init - Initialize the malloc package.  Called once before any other
// calls are made.

// This simple implementation initializes some large chunk of memory right off the
// bat by sbrk

#ifndef INITIAL_SBRK_SIZE
#define INITIAL_SBRK_SIZE (8200)
#endif

int my_init() {
  chunk_t* first_chunk = mem_sbrk(INITIAL_SBRK_SIZE);
  assert(IS_ALIGNED(first_chunk));
  mem_sbrk(sizeof(size_int));
  first_chunk->current_size = INITIAL_SBRK_SIZE - sizeof(size_int);
  SET_PREVIOUS_INUSE(first_chunk);
  END_OF_HEAP_BIN = first_chunk;
  return 0;
}


/* ------------------------------------------------------------------------- */
// [BEGIN CHUNK INSERT/REMOVE METHODS]
static void* small_malloc(size_int request) {

}

static void* large_malloc(size_int request) {

}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.

// Pseudocode - to malloc a block, first round up the requested size to the nearest
// multiple of alignment, or SMALLEST_MALLOC, whichever is larger. If this requested size is smaller
// than the threshold, malloc a small chunk. If this requested size is larger than the
// threshold, malloc a large chunk.

#define MAX(a, b) ((a) ^ (((a) ^ (b)) & -((a) < (b))))

void * my_malloc(size_t size) {
  if (size == 0)
    return NULL;
  size_int aligned_size = ALIGN(size);
  size_int request = MAX(aligned_size, SMALLEST_MALLOC);
  if (IS_LARGE_SIZE(request))
    return large_malloc(request);
  else
    return small_malloc(request);
}
// [END MALLOC METHODS]
/* ------------------------------------------------------------------------- */

// free - Freeing a block does nothing.

/*
Pseudocode - to free a ptr, first check if the previous and next chunks are free.

DON'T CHECK THE NEXT CHUNK IF THE CURRENT CHUNK IS THE LAST CHUNK.

If they are, remove them from their bins so that we can combine with them safely.

If either the front or back pointers were the designated victim, set the victim bin to NULL

Then, combine with them so we get one large chunk.

If this chunk (possibly combined now) is at the end of the heap, replace the END_OF_HEAP_BIN
with this chunk, and call it a day.

Otherwise, insert this new chunk into the corresponding bin.
*/

#define IS_END_OF_HEAP(chunk_ptr) ((chunk_ptr) == *END_OF_HEAP_BIN)
#define CAN_COMBINE_PREVIOUS(chunk_ptr) (!IS_PREVIOUS_INUSE(chunk_ptr))
#define CAN_COMBINE_NEXT(chunk_ptr) (!IS_END_OF_HEAP(chunk_ptr) && !IS_CURRENT_INUSE(NEXT_HEAP_CHUNK(chunk_ptr)))

void my_free(void *ptr) {

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
