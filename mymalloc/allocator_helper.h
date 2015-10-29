#include <stdint.h>

#ifndef _ALLOCATOR_STRUCTS_H
#define _ALLOCATOR_STRUCTS_H

#define CURRENT_CHUNK_INUSE 1
#define PREVIOUS_CHUNK_INUSE 2

#define IS_PREVIOUS_INUSE(chunk_ptr) (((chunk_ptr)->current_size) & PREVIOUS_CHUNK_INUSE)
#define IS_CURRENT_INUSE(chunk_ptr) (((chunk_ptr)->current_size) & CURRENT_CHUNK_INUSE)

#define SET_PREVIOUS_INUSE(chunk_ptr) (chunk_ptr)->current_size |= PREVIOUS_CHUNK_INUSE;
#define SET_CURRENT_INUSE(chunk_ptr) (chunk_ptr)->current_size |= CURRENT_CHUNK_INUSE;

#define SAFE_SIZE(size) ((size) & ~7ULL)

typedef uint64_t size_int;

#define SMALLEST_MALLOC (2*sizeof(struct small_chunk*)+sizeof(size_int))
#define SMALLEST_CHUNK (SMALLEST_MALLOC + sizeof(size_int))

#define USERSPACE_SIZE(ptr) (SAFE_SIZE((ptr)->current_size) - sizeof(size_int))

#define USER_POINTER_TO_CHUNK(ptr) ((struct small_chunk*) (((uint64_t) (ptr)) - 2*sizeof(size_int)))

#define PREVIOUS_HEAP_CHUNK(ptr) ((struct small_chunk*) (((uint64_t) (ptr)) - SAFE_SIZE((ptr)->previous_size)))
#define NEXT_HEAP_CHUNK(ptr) ((struct small_chunk*) (((uint64_t) (ptr)) + SAFE_SIZE((ptr)->current_size)))

#define CAN_COMBINE_PREVIOUS(ptr) (!IS_PREVIOUS_INUSE(ptr))
#define CAN_COMBINE_NEXT(ptr) (!IS_CURRENT_INUSE(NEXT_HEAP_CHUNK(ptr)))

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
  struct large_chunk* left; //Corresponds to the left child in the binary tree
  struct large_chunk* right; //Corresponds to the right child in the binary tree
};

typedef struct small_chunk chunk_t;
typedef struct large_chunk bigchunk_t;

#endif  // _ALLOCATOR_STRUCTS_H
