#include "./my_checker.h"

int my_checker(chunk_t** bins, int length) {
  if (sizeof(chunk_t*)*length > 512)
    return 1;
  return 0;
}
