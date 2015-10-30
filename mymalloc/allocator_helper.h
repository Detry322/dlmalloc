#include <stdint.h>

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

typedef uint64_t size_int;

#define SMALLEST_MALLOC (2*sizeof(struct small_chunk*)+sizeof(size_int))
#define SMALLEST_CHUNK (SMALLEST_MALLOC + sizeof(size_int))

#define USER_POINTER_TO_CHUNK(ptr) ((struct small_chunk*) (((uint64_t) (ptr)) - 2*sizeof(size_int)))
#define CHUNK_TO_USER_POINTER(chunk_ptr) ((void*) &(ptr)->next)

#define PREVIOUS_HEAP_CHUNK(chunk_ptr) ((struct small_chunk*) \
              (((uint64_t) (chunk_ptr)) - SAFE_SIZE((chunk_ptr)->previous_size) - sizeof(size_int)))

#define NEXT_HEAP_CHUNK(chunk_ptr) ((struct small_chunk*) \
              (((uint64_t) (chunk_ptr)) + SAFE_SIZE((chunk_ptr)->current_size) + sizeof(size_int)))

#define CIRCULAR_LIST_IS_LENGTH_ONE(chunk_ptr) ((chunk_ptr) == (chunk_ptr)->next && (chunk_ptr) == (chunk_ptr)->prev)

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
  unsigned int mask; // The shift we need to do to decide if we go left or right down this node
                      // For 256-511 (10D000000), this would be 6 since we need to shift 6 to the
                      // left to decide to use the left or right
                      // for 1024-2047 (10D00000000) This shift would be 8
                      // Then, as you go down the tree, you subtract one.
};

#define NO_PARENT_CIRCLE_NODE NULL
#define NO_PARENT_ROOT_NODE ((struct large_chunk*) 0x1)

typedef struct small_chunk chunk_t;
typedef struct large_chunk bigchunk_t;

#endif  // _ALLOCATOR_STRUCTS_H
