#include "slicer.h"

#include <math.h>

void slicer_insert_before(TWorld *w, int32_t idx, int32_t before) {
    TCell *nc = &w->cells[idx];
    TCell *bc = &w->cells[before];
    int32_t prev = bc->p_time;

    nc->p_time = prev;
    nc->n_time = before;
    w->cells[prev].n_time = idx;
    bc->p_time = idx;
}

void slicer_remove(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];
    int32_t n = c->n_time, p = c->p_time;

    if (idx == w->ce)
        w->ce = (n == idx) ? -1 : n;

    if (n != idx) {
        w->cells[p].n_time = n;
        w->cells[n].p_time = p;
    }
    c->n_time = c->p_time = idx;
}

void slicer_advance(TWorld *w) {
    if (w->ce >= 0)
        w->ce = w->cells[w->ce].n_time;
}

int32_t slicer_slice_size(TWorld *w, const TCell *c) {
    const TConfig *cfg = &w->cfg;

    double base;
    if (cfg->size_dep_slice)
        base = (cfg->slice_pow == 1.0) ? (double)c->mm_size : pow((double)c->mm_size, cfg->slice_pow);
    else
        base = (double)cfg->slice_size;

    int32_t fixed = (int32_t)(cfg->slic_fix_frac * base);
    uint32_t ran_range = (uint32_t)(cfg->slic_ran_frac * base) + 1;
    return fixed + (int32_t)rng_below(&w->rng, ran_range);
}
