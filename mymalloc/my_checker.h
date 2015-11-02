#include <assert.h>
#include <stdlib.h>
#include "./allocator_helper.h"
#include "./memlib.h"

#ifndef _MY_CHECKER_H
#define _MY_CHECKER_H

int my_checker(chunk_t** bins, int length);
bool is_valid_pointer_tree(int i, bigchunk_t* chunk);
bool chunk_not_in_tree(bigchunk_t* root, bigchunk_t* chunk);
bool chunk_in_tree(bigchunk_t* root, bigchunk_t* chunk);

#endif  // _MY_CHECKER_H
