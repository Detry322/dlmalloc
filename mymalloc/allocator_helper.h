#include <stdint.h>
#include <stdbool.h>

#ifndef _ALLOCATOR_STRUCTS_H
#define _ALLOCATOR_STRUCTS_H

#define CURRENT_CHUNK_INUSE 1ULL
#define PREVIOUS_CHUNK_INUSE 2ULL

#define IS_PREVIOUS_INUSE(chunk_ptr) (((chunk_ptr)->current_size) & PREVIOUS_CHUNK_INUSE)
#define IS_CURRENT_INUSE(chunk_ptr) (((chunk_ptr)->current_size) & CURRENT_CHUNK_INUSE)
#define IS_PREVIOUS_FREE(chunk_ptr) (!IS_PREVIOUS_INUSE(chunk_ptr))
#define IS_CURRENT_FREE(chunk_ptr) (!IS_CURRENT_INUSE(chunk_ptr))

#define SET_PREVIOUS_INUSE(chunk_ptr) (chunk_ptr)->current_size |= PREVIOUS_CHUNK_INUSE;
#define SET_CURRENT_INUSE(chunk_ptr) (chunk_ptr)->current_size |= CURRENT_CHUNK_INUSE;

#define CLEAR_PREVIOUS_INUSE(chunk_ptr) (chunk_ptr)->current_size &= ~PREVIOUS_CHUNK_INUSE;
#define CLEAR_CURRENT_INUSE(chunk_ptr) (chunk_ptr)->current_size &= ~CURRENT_CHUNK_INUSE;

#define SAFE_SIZE(size) ((size) & ~7ULL)
#define CHUNK_SIZE(chunk_ptr) (SAFE_SIZE((chunk_ptr)->current_size))
#define CHUNK_SIZES_EQUAL(chunk_ptr1, chunk_ptr2) (CHUNK_SIZE(chunk_ptr1) == CHUNK_SIZE(chunk_ptr2))

#define FAST_LOG2(x) (sizeof(unsigned long long)*8 - 1 - __builtin_clzll((unsigned long long)(x)))
typedef uint64_t size_int;

#define SMALLEST_MALLOC (2*sizeof(struct small_chunk*)+sizeof(size_int))
#define SMALLEST_CHUNK (SMALLEST_MALLOC + sizeof(size_int))

#define USER_POINTER_TO_CHUNK(ptr) ((struct small_chunk*) (((uint64_t) (ptr)) - 2*sizeof(size_int)))
#define CHUNK_TO_USER_POINTER(chunk_ptr) ((void*) chunk_ptr + 2*sizeof(size_int))

#define PREVIOUS_HEAP_CHUNK(chunk_ptr) ((struct small_chunk*) \
              (((uint64_t) (chunk_ptr)) - SAFE_SIZE((chunk_ptr)->previous_size) - sizeof(size_int)))

#define NEXT_HEAP_CHUNK(chunk_ptr) ((struct small_chunk*) \
              (((uint64_t) (chunk_ptr)) + SAFE_SIZE((chunk_ptr)->current_size) + sizeof(size_int)))

#define IS_END_OF_HEAP(chunk_ptr) ((chunk_ptr) == USER_POINTER_TO_CHUNK(mem_heap_hi() + 1 - CHUNK_SIZE(chunk_ptr)))

#define CIRCULAR_LIST_IS_LENGTH_ONE(chunk_ptr) ((chunk_ptr) == (chunk_ptr)->next && (chunk_ptr) == (chunk_ptr)->prev)

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

//size refers to the size of the chunk, not the malloc.
struct small_chunk {
  size_int previous_size; //Only valid if the PREVIOUS_INUSE bit is not set
  // Otherwise, currently being used by the previous chunk to store data;

  size_int current_size;// most significant 61 bits correspond to size, least significant 3 store information
  // This next pointer is the start of usable memory for the malloc-caller.
  struct small_chunk* next;
  struct small_chunk* prev;
};

struct large_chunk {
  size_int previous_size;
  size_int current_size;
  struct large_chunk* next;
  struct large_chunk* prev;
  struct large_chunk* children[2]; //Corresponds this chunk's children.
  struct large_chunk* parent; //Corresponds to the parent in the binary tree
  unsigned int bin_number; // The bin this child is in
  unsigned int shift; // The shift we need to do to decide if we go left or right down this node
                      // For 256-511 (10D000000), this would be 6 since we need to shift 6 to the
                      // left to decide to use the left or right
                      // for 1024-2047 (10D00000000) This shift would be 8
                      // Then, as you go down the tree, you subtract one.
};

#define NO_PARENT_CIRCLE_NODE NULL
#define NO_PARENT_ROOT_NODE ((struct large_chunk*) 0x1)

#define IS_VALID_LARGE_CHUNK(chunk_ptr) \
  (IS_CURRENT_FREE(chunk_ptr) && (chunk_ptr)->next != NULL && (chunk_ptr)->prev != NULL \
    && ((chunk_ptr)->parent == NO_PARENT_ROOT_NODE || (chunk_ptr)->parent == NO_PARENT_CIRCLE_NODE || \
    ((chunk_ptr)->parent->children[(CHUNK_SIZE(chunk_ptr) >> (chunk_ptr)->parent->shift) & 1] == (chunk_ptr)) \
    ))

#define CONTAINS_TREE_LOOPS(chunk_ptr) \
  ((chunk_ptr)->parent == (chunk_ptr) || (chunk_ptr) == (chunk_ptr)->children[1] || \
    (chunk_ptr) == (chunk_ptr)->children[0] || ((chunk_ptr)->children[1] == (chunk_ptr)->children[0] && \
        (chunk_ptr)->children[0] != NULL))


#define IS_VALID_SMALL_CHUNK(chunk_ptr) \
  (IS_CURRENT_FREE(chunk_ptr) && (chunk_ptr)->next != NULL && (chunk_ptr)->prev != NULL)

typedef struct small_chunk chunk_t;
typedef struct large_chunk bigchunk_t;

#endif  // _ALLOCATOR_STRUCTS_H
