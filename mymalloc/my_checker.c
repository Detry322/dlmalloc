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

bool is_valid_chunk_pointer(chunk_t* chunk) {
  uint64_t offset = (uint64_t) mem_heap_lo() & 7;
  chunk_t* start = mem_heap_lo() + offset;
  while (true) {
    if (start == chunk)
      return true;
    if (start > chunk)
      return false;
    start = NEXT_HEAP_CHUNK(start);
  }
}

bool chunk_not_in_tree(bigchunk_t* root, bigchunk_t* chunk) {
  if (root == NULL)
    return true;
  if (root == chunk)
    return false;
  bigchunk_t* current = root->next;
  while (current != root) {
    if (current == chunk)
      return false;
    current = current->next;
  }
  return chunk_not_in_tree(root->children[0], chunk) && chunk_not_in_tree(root->children[1], chunk);
}

bool chunk_in_tree(bigchunk_t* root, bigchunk_t* chunk) {
  if (root == NULL)
    return false;
  if (root == chunk)
    return true;
  bigchunk_t* current = root->next;
  while (current != root) {
    if (current == chunk)
      return true;
    current = current->next;
  }
  return chunk_in_tree(root->children[0], chunk) || chunk_in_tree(root->children[1], chunk);
}

bool is_valid_pointer_tree(int i, bigchunk_t* chunk) {
  if (chunk == NULL)
    return true;
  assert(IS_VALID_LARGE_CHUNK(chunk));
  bool is_valid = true;
  if (CONTAINS_TREE_LOOPS(chunk))
    is_valid = false;
  assert(is_valid);
  if (chunk->children[0] != NULL && chunk->children[0]->parent != chunk)
    is_valid = false;
  assert(is_valid);
  if (chunk->children[1] != NULL && chunk->children[1]->parent != chunk)
    is_valid = false;
  assert(is_valid);
  if (chunk->bin_number != i)
    is_valid = false;
  assert(is_valid);
  if (!is_valid_pointer_tree(i, chunk->children[0]))
    is_valid = false;
  assert(is_valid);
  if (!is_valid_pointer_tree(i, chunk->children[1]))
    is_valid = false;
  assert(is_valid);
  return is_valid;
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
    printf("Chunk %d%s: %p\n", chunks, (IS_END_OF_HEAP(chunk)) ? " (END OF HEAP)" : "", chunk);
    printf("Previous Size: %llu %s\n", chunk->previous_size, (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("Current Size:  %llu\n", CHUNK_SIZE(chunk));
    printf("PREV_INUSE:    %llu\n", IS_PREVIOUS_INUSE(chunk));
    printf("CURRENT_INUSE: %llu\n", IS_CURRENT_INUSE(chunk));
    printf("Next:          %p %s\n", chunk->next, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("Prev:          %p %s\n", chunk->prev, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("NEXT_HEAP_CHK: %p\n", NEXT_HEAP_CHUNK(chunk));
    printf("PREV_HEAP_CHK: %p %s\n", PREVIOUS_HEAP_CHUNK(chunk), (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
    printf("-------\n");
    #endif
    if (IS_END_OF_HEAP(chunk))
      break;

    assert(chunk == USER_POINTER_TO_CHUNK(CHUNK_TO_USER_POINTER(chunk)));
    chunks++;
    if (!IS_VICTIM(chunk) && !IS_CURRENT_INUSE(chunk)) {
      assert(is_circularly_linked_list(chunk));
      if (IS_LARGE_CHUNK(chunk) && !IS_HUGE_CHUNK(chunk)) {
        assert(IS_VALID_LARGE_CHUNK((bigchunk_t*)chunk));
      } else if (IS_SMALL_CHUNK(chunk)) {
        assert(IS_VALID_SMALL_CHUNK(chunk));
      }
      assert(is_valid_chunk_pointer(chunk->next));
      assert(is_valid_chunk_pointer(chunk->prev));
      assert(is_valid_chunk_pointer(chunk));
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
  for (int i = 32; i < 64; i++)
    assert(is_valid_pointer_tree(i, (bigchunk_t*) bins[i]));
  #ifdef VERBOSE
  printf("------------------------------------------------------\n");
  printf("Check #%d: Checked %d chunks.\n", checks, chunks);
  printf("------------------------------------------------------\n");
  #endif
  return 0;
}
