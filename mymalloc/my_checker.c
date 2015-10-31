#include "./my_checker.h"
#include <stdio.h>

int my_checker(chunk_t** bins, int length) {
  if (sizeof(chunk_t*)*length > 512)
    return 1;
  printf("Memory: ");
  for (uint64_t* i = mem_heap_lo(); i < (uint64_t*) bins[0] + 2; i++) {
    printf("%llx ", *i);
  }
  printf("\n");
  return 0;
}
