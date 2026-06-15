#include "soup.h"

#include <stdlib.h>
#include <string.h>

static void free_list_grow(TSoup *s) {
    int32_t cap = s->free_cap ? s->free_cap * 2 : 16;
    s->free = realloc(s->free, (size_t)cap * sizeof(TFreeBlock));
    s->free_cap = cap;
}

static void free_list_insert(TSoup *s, int32_t idx, TFreeBlock b) {
    if (s->free_n == s->free_cap)
        free_list_grow(s);
    memmove(&s->free[idx + 1], &s->free[idx],
            (size_t)(s->free_n - idx) * sizeof(TFreeBlock));
    s->free[idx] = b;
    s->free_n++;
}

static void free_list_remove(TSoup *s, int32_t idx) {
    memmove(&s->free[idx], &s->free[idx + 1],
            (size_t)(s->free_n - idx - 1) * sizeof(TFreeBlock));
    s->free_n--;
}

void soup_init(TSoup *s, const TConfig *cfg) {
    s->size = cfg->soup_size;
    s->min_cell_size = cfg->min_cell_size;
    s->mal_mode = cfg->mal_mode;
    s->mem = calloc((size_t)s->size, 1); /* dead memory = nop0 = 0x00 */

    s->free = NULL;
    s->free_n = 0;
    s->free_cap = 0;
    free_list_grow(s);
    s->free[0] = (TFreeBlock){.addr = 0, .size = s->size};
    s->free_n = 1;
}

void soup_destroy(TSoup *s) {
    free(s->mem);
    free(s->free);
    s->mem = NULL;
    s->free = NULL;
}

int32_t soup_alloc(TSoup *s, int32_t size, int32_t hint) {
    (void)hint; /* unused by first-fit and better-fit */

    int32_t best = -1;
    for (int32_t i = 0; i < s->free_n; i++) {
        if (s->free[i].size < size)
            continue;
        if (s->mal_mode == 0) { /* first-fit: leftmost adequate block */
            best = i;
            break;
        }
        /* better-fit: smallest adequate block, ties -> leftmost */
        if (best < 0 || s->free[i].size < s->free[best].size)
            best = i;
    }
    if (best < 0)
        return -1;

    int32_t addr = s->free[best].addr;
    if (s->free[best].size == size)
        free_list_remove(s, best);
    else {
        s->free[best].addr += size;
        s->free[best].size -= size;
    }
    return addr;
}

void soup_free(TSoup *s, int32_t addr, int32_t size) {
    /* find insertion point: first block with addr greater than ours */
    int32_t idx = 0;
    while (idx < s->free_n && s->free[idx].addr < addr)
        idx++;

    int merge_left = (idx > 0 && s->free[idx - 1].addr + s->free[idx - 1].size == addr);
    int merge_right = (idx < s->free_n && addr + size == s->free[idx].addr);

    if (merge_left && merge_right) {
        s->free[idx - 1].size += size + s->free[idx].size;
        free_list_remove(s, idx);
    } else if (merge_left) {
        s->free[idx - 1].size += size;
    } else if (merge_right) {
        s->free[idx].addr = addr;
        s->free[idx].size += size;
    } else {
        free_list_insert(s, idx, (TFreeBlock){.addr = addr, .size = size});
    }
}

int32_t soup_free_total(const TSoup *s) {
    int32_t total = 0;
    for (int32_t i = 0; i < s->free_n; i++)
        total += s->free[i].size;
    return total;
}
