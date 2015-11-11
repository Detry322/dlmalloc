#include "./my_checker.h"
#include <stdio.h>
#include <stdbool.h>

bool is_circularly_linked_list(chunk_t* chunk) {
  // If they aren't circularly linked, this will segfault.
  chunk_t* other = chunk->next;
  int left = 0;
  while (other != chunk) {
    left++;
    other = other->next;
  }
  int right = 0;
  other = chunk->prev;
  while (other != chunk) {
    right++;
    other = other->prev;
  }
  return left == right;
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
  return !chunk_not_in_tree(root, chunk);
}

bool is_valid_pointer_tree(int i, bigchunk_t* chunk) {
  if (chunk == NULL)
    return true;
  if (CONTAINS_TREE_LOOPS(chunk))
    return false;
  if (chunk->children[0] != NULL && chunk->children[0]->parent != chunk)
    return false;
  if (chunk->children[1] != NULL && chunk->children[1]->parent != chunk)
    return false;
  if (chunk->bin_number != i)
    return false;
  if (!is_valid_pointer_tree(i, chunk->children[0]))
    return false;
  if (!is_valid_pointer_tree(i, chunk->children[1]))
    return false;
  return true;
}

void print_chunk_summary(chunk_t** bins, chunk_t* chunk) {
  printf("Chunk %p%s:\n", chunk, (IS_END_OF_HEAP(chunk)) ? " (END OF HEAP)" : "");
  printf("Previous Size: %llu %s\n", chunk->previous_size, (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
  printf("Current Size:  %llu\n", CHUNK_SIZE(chunk));
  printf("PREV_INUSE:    %llu\n", IS_PREVIOUS_INUSE(chunk));
  printf("CURRENT_INUSE: %llu\n", IS_CURRENT_INUSE(chunk));
  printf("Next:          %p %s\n", chunk->next, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
  printf("Prev:          %p %s\n", chunk->prev, (IS_CURRENT_INUSE(chunk)) ? " (Can't trust)" : "");
  printf("NEXT_HEAP_CHK: %p\n", NEXT_HEAP_CHUNK(chunk));
  printf("PREV_HEAP_CHK: %p %s\n", PREVIOUS_HEAP_CHUNK(chunk), (IS_PREVIOUS_INUSE(chunk)) ? " (Can't trust)" : "");
  printf("-------\n");
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
  while (true) {
    #ifdef VERBOSE
    print_chunk_summary(bins, chunk);
    #endif

    if (IS_END_OF_HEAP(chunk))
      break;

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
  return 0;
}
