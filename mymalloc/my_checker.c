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

  // First, do a run-through of the heap
  // If this segfaults, then the IS_END_OF_HEAP macro is incorrect.
  uint64_t offset = (uint64_t) mem_heap_lo() & 7;
  chunk_t* chunk = mem_heap_lo() + offset;
  int chunks = 1;
  while (true) {
    #ifdef VERBOSE
    printf("Chunk %d%s: 0x%llx\n", chunks, (IS_END_OF_HEAP(chunk)) ? " (END OF HEAP)" : "", chunk);
    printf("Previous Size: %ld %s\n", chunk->previous_size, (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("Current Size:  %ld\n", CHUNK_SIZE(chunk));
    printf("PREV_INUSE:    %d\n", IS_PREVIOUS_INUSE(chunk));
    printf("CURRENT_INUSE: %d\n", IS_CURRENT_INUSE(chunk));
    printf("Next:          0x%llx %s\n", chunk->next, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("Prev:          0x%llx %s\n", chunk->prev, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("NEXT_HEAP_CHK: 0x%llx\n", NEXT_HEAP_CHUNK(chunk));
    printf("PREV_HEAP_CHK: 0x%llx %s\n", PREVIOUS_HEAP_CHUNK(chunk), (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("-------\n");
    #endif
    if (IS_END_OF_HEAP(chunk))
      break;

    assert(chunk == USER_POINTER_TO_CHUNK(CHUNK_TO_USER_POINTER(chunk)));
    chunks++;
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
    assert(chunk->current_size > 4);
    chunk = NEXT_HEAP_CHUNK(chunk);
  }
  assert(IS_PREVIOUS_INUSE(chunk));
  #ifdef VERBOSE
  printf("------------------------------------------------------\n");
  printf("Check #%d: Checked %d chunks.\n", checks, chunks);
  printf("------------------------------------------------------\n");
  #endif
  return 0;
}
