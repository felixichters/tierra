#include "reaper.h"
#include "slicer.h"

/* Legacy NumCellsMin: the reaper never reduces the population below this
 * floor. */
#define NUM_CELLS_MIN 1

void reaper_insert_bottom(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];

    c->p_reap = w->bottom_reap;
    c->n_reap = -1;
    if (w->bottom_reap >= 0)
        w->cells[w->bottom_reap].n_reap = idx;
    else
        w->top_reap = idx;
    w->bottom_reap = idx;
}

void reaper_remove(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];
    int32_t n = c->n_reap, p = c->p_reap;

    if (p >= 0)
        w->cells[p].n_reap = n;
    else
        w->top_reap = n;

    if (n >= 0)
        w->cells[n].p_reap = p;
    else
        w->bottom_reap = p;

    c->n_reap = c->p_reap = -1;
}

/* Swap `idx` with its upward (toward top_reap) neighbour. No-op if `idx`
 * is already at the top. */
static void swap_up(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];
    int32_t up = c->p_reap;
    if (up < 0)
        return;

    TCell *u = &w->cells[up];
    int32_t up_up = u->p_reap;
    int32_t down = c->n_reap;

    c->p_reap = up_up;
    c->n_reap = up;
    u->p_reap = idx;
    u->n_reap = down;

    if (up_up >= 0)
        w->cells[up_up].n_reap = idx;
    else
        w->top_reap = idx;

    if (down >= 0)
        w->cells[down].p_reap = up;
    else
        w->bottom_reap = up;
}

void reaper_up_if(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];
    if (c->p_reap >= 0 && c->err_flags >= w->cells[c->p_reap].err_flags)
        swap_up(w, idx);
}

void reaper_down_if(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];
    /* Swapping idx down past its successor is the same as swapping that
     * successor up past idx. */
    if (c->n_reap >= 0 && c->err_flags <= w->cells[c->n_reap].err_flags)
        swap_up(w, c->n_reap);
}

void reap_cell(TWorld *w, int32_t idx) {
    TCell *c = &w->cells[idx];

    soup_free(&w->soup, c->mm_addr, c->mm_size);
    w->sum_mm_size -= c->mm_size;
    if (c->md_size > 0)
        soup_free(&w->soup, c->md_addr, c->md_size);

    genome_release(&w->gb, c->genotype);

    slicer_remove(w, idx);
    reaper_remove(w, idx);

    c->alive = 0;
    w->free_cells[w->free_n++] = idx;
    w->num_cells--;
    w->deaths++;
}

int reaper_reap(TWorld *w, int ex, int32_t ce_idx) {
    if (w->num_cells <= NUM_CELLS_MIN)
        return 1;

    int32_t reap_range = (int32_t)(w->cfg.reap_rnd_prop * (double)w->num_cells);

    int32_t cr = w->top_reap;
    if (reap_range >= 2) {
        uint32_t steps = rng_below(&w->rng, (uint32_t)reap_range);
        for (uint32_t i = 0; i < steps; i++) {
            int32_t next = w->cells[cr].n_reap;
            if (next < 0)
                break;
            cr = next;
        }
    }

    if (ex && cr == ce_idx) {
        int32_t alt = (cr == w->top_reap) ? w->cells[cr].n_reap : w->cells[cr].p_reap;
        if (alt < 0)
            return 1;
        cr = alt;
    }

    reap_cell(w, cr);
    return 0;
}

void reaper_lazy_cull(TWorld *w) {
    if (w->cfg.lazy_tol <= 0 || w->num_divides == 0)
        return;

    double threshold = w->avg_repinst * (double)w->cfg.lazy_tol;
    while (w->num_cells > NUM_CELLS_MIN && w->ce >= 0 &&
           (double)w->cells[w->ce].repinst > threshold)
        reap_cell(w, w->ce); /* slicer_remove (via reap_cell) advances w->ce */
}
