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

#define LOGARITHMIC_START 249
// Anything 249 bytes or larger will go in the "large" bins

#define HUGE_BIN_CUTOFF 16777215
// Anything larger than this goes in the huge bin

#define END_OF_HEAP_BIN (bins[0])
#define VICTIM_BIN (bins[1])
#define HUGE_BIN (bins[2])

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.

int my_check() {
  return my_checker(bins, NUM_OF_BINS);
}



// init - Initialize the malloc package.  Called once before any other
// calls are made.

// This simple implementation initializes some large chunk of memory right off the
// bat by sbrk

#ifndef INITIAL_SBRK_SIZE
#define INITIAL_SBRK_SIZE 8192
#endif

int my_init() {
  chunk_t* first_chunk = mem_sbrk(INITIAL_SBRK_SIZE);
  assert(IS_ALIGNED(first_chunk));
  mem_sbrk(sizeof(size_int));
  first_chunk->current_size = INITIAL_SBRK_SIZE;
  SET_CURRENT_INUSE(first_chunk);
  END_OF_HEAP_BIN = first_chunk;
  return 0;
}

#define CIRCULAR_LIST_IS_LENGTH_ONE(chunk) ((chunk) == (chunk)->next && (chunk) == (chunk)->prev)

// This unlinks a chunk from the circularly linked list
// Returns 0 if the circularly linked list has more elements
// Returns 1 if the circularly linked list ran out of elements
static int unlink_chunk(chunk_t** chunk_ptr) {
  chunk_t* chunk = *chunk_ptr;
  if (CIRCULAR_LIST_IS_LENGTH_ONE(chunk)) {
    *chunk_ptr = NULL;
    return 1;
  }
  chunk->prev->next = chunk->next;
  chunk->next->prev = chunk->prev;
  *chunk_ptr = chunk->next;
  return 0;
}

static int unlink_bigchunk(bigchunk_t** chunk_ptr) {
  bigchunk_t* chunk = *chunk_ptr;
  if (unlink_chunk((chunk_t**) chunk_ptr)) {
    //The chunk ran out of elements in the circularly linked list, so we have to
    //re-connect the tree

    //Somehow randomly decide whether to use the right most child of the left
    //or the left most child of the right, for now, always use the left

    //TODO
    return 1;
  }
  return 0;
}

//Adds the chunk to the circularly linked list
// Returns 0 if the circularly linked list already had chunks in it
// Returns 1 if the circularly linked list had no chunks in it
static int link_chunk(chunk_t** old_chunk_ptr, chunk_t* new_chunk) {
  chunk_t* old_chunk = *old_chunk_ptr;
  if (old_chunk == NULL) {
    new_chunk->next = new_chunk;
    new_chunk->prev = new_chunk;
    *old_chunk_ptr = new_chunk;
    return 1;
  }
  old_chunk->prev->next = new_chunk;
  old_chunk->prev = new_chunk;
  *old_chunk_ptr = new_chunk;
  return 0;
}

static int link_bigchunk(bigchunk_t** old_chunk_ptr, bigchunk_t* new_chunk) {
  //TODO
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void * my_malloc(size_t size) {
  return NULL;
}

// free - Freeing a block does nothing.
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
  copy_size = USERSPACE_SIZE(USER_POINTER_TO_CHUNK(ptr));

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
