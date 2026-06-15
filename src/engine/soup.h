#ifndef TIERRA_SOUP_H
#define TIERRA_SOUP_H

#include <stdint.h>
#include "tierra/tierra.h"

/* A free block of soup memory, [addr, addr+size). */
typedef struct TFreeBlock {
    int32_t addr;
    int32_t size;
} TFreeBlock;

/* The soup: a flat byte array of instructions plus a sorted, merged
 * free-block list. Replaces the legacy Cartesian-tree allocator
 * (memtree.c) with a simple list -- first-fit / better-fit is all that
 * MalMode 0/1 actually need. */
typedef struct TSoup {
    uint8_t *mem;
    int32_t  size;
    int32_t  min_cell_size;
    int32_t  mal_mode;     /* 0 = first-fit, 1 = better-fit (see soup_alloc) */

    TFreeBlock *free;      /* sorted by addr, ascending, non-overlapping */
    int32_t     free_n;
    int32_t     free_cap;
} TSoup;

void soup_init(TSoup *s, const TConfig *cfg);
void soup_destroy(TSoup *s);

/* Allocate a block of `size` instructions.
 * mode 0 (first-fit):  the leftmost free block large enough.
 * mode 1 (better-fit): the smallest free block large enough (ties -> leftmost).
 * `hint` is accepted for API symmetry with legacy mal() but unused by modes
 * 0/1 (the only modes the default config exercises).
 * Returns the allocated address, or -1 if no block is large enough. */
int32_t soup_alloc(TSoup *s, int32_t size, int32_t hint);

/* Return [addr, addr+size) to the free list, merging with neighbours. */
void soup_free(TSoup *s, int32_t addr, int32_t size);

/* Total free space across all blocks. */
int32_t soup_free_total(const TSoup *s);

#endif /* TIERRA_SOUP_H */
