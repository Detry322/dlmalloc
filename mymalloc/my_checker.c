#include "./my_checker.h"
#include <stdio.h>
#include <stdbool.h>

bool is_circularly_linked_list(chunk_t* chunk) {
  bool is_right_linked = false;
  bool is_left_linked = false;
  chunk_t* other = chunk->next;
  for (int i = 0; i < 100000; i++) {
    if (chunk == other) {
      is_right_linked = true;
      break;
    }
    other = other->next;
  }
  other = chunk->prev;
  for (int i = 0; i < 100000; i++) {
    if (chunk == other) {
      is_left_linked = true;
      break;
    }
    other = other->prev;
  }
  return is_right_linked && is_left_linked;
}

int my_checker(chunk_t** bins, int length) {
  static int checks = 0;
  checks++;
  if (sizeof(chunk_t*)*length > 512)
    return 1;
  // printf("Memory: ");
  // for (uint64_t* i = mem_heap_lo(); i < (uint64_t*) bins[0] + 2; i++) {
  //   printf("%llx ", *i);
  // }
  // printf("\n");

  // First, do a run-through of the heap
  // If this segfaults, then the IS_END_OF_HEAP macro is incorrect.
  uint64_t offset = (uint64_t) mem_heap_lo() & 7;
  chunk_t* chunk = mem_heap_lo() + offset;
  while (!IS_END_OF_HEAP(chunk)) {
    if (chunk != bins[1] && !IS_CURRENT_INUSE(chunk)) {
      assert(is_circularly_linked_list(chunk));
    }
    if (!IS_CURRENT_INUSE(chunk)) {
      assert(chunk == PREVIOUS_HEAP_CHUNK(NEXT_HEAP_CHUNK(chunk)));
      assert(CHUNK_SIZE(chunk) == NEXT_HEAP_CHUNK(chunk)->previous_size);
      assert(!IS_PREVIOUS_INUSE(NEXT_HEAP_CHUNK(chunk)));
    } else {
      assert(IS_PREVIOUS_INUSE(NEXT_HEAP_CHUNK(chunk)));
    }
    chunk = NEXT_HEAP_CHUNK(chunk);
  }
  assert(IS_PREVIOUS_INUSE(chunk));
  return 0;
}
