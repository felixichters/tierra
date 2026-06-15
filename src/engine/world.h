#ifndef TIERRA_WORLD_H
#define TIERRA_WORLD_H

#include <stdint.h>
#include <time.h>

#include "tierra/tierra.h"
#include "soup.h"
#include "cpu.h"
#include "rng.h"
#include "genome.h"

/* Genetic-operator constants from tierra/soup_in (not user-tunable).
 * MAX_MAL_MULT is defined in cpu.h since exec_mal (isa.c) needs it too. */
#define MOV_PROP_THR_DIV 0.7 /* MovPropThrDiv: min fraction of a daughter cell that must be genome */
#define MATE_SIZE_EP     2   /* MateSizeEp: size tolerance, in bytes, for same-size crossover mates */

/* One cell: a CPU plus the demographic/memory bookkeeping the legacy engine
 * keeps in Dem/Mem/Que (tierra.h:580-628). Cells live in a fixed-capacity
 * array (TWorld::cells); slicer and reaper queues are doubly-linked via
 * indices stored in the cell itself, so the array never needs to move. */
typedef struct TCell {
    TCpu cpu;

    int32_t mm_addr, mm_size;  /* this cell's own memory block (ce->mm) */
    int32_t md_addr, md_size;  /* pending daughter block from mal (ce->md) */

    int32_t mg_off, mg_size;       /* genetic-memory region within mm (d.mg) */
    int32_t mov_off_min, mov_off_max; /* d.MovOffMin / d.MovOffMax */
    int32_t mov_daught;            /* d.mov_daught */

    uint64_t inst;       /* age: total instructions executed (d.inst) */
    uint64_t repinst;    /* instructions since last successful divide */
    int32_t  fecundity;
    int32_t  err_flags;  /* cumulative E-flag count (d.flags); reaper rank */
    int32_t  generation; /* parent's generation + 1; ancestor = 0 */

    int32_t genotype;    /* genebank id, -1 until first registered */

    int64_t ib;          /* instruction-bank slice carryover (c.ib) */

    int alive;

    /* slicer: circular doubly-linked queue over live cells */
    int32_t n_time, p_time;

    /* reaper: linear doubly-linked queue, top (worst) -> bottom (safest) */
    int32_t n_reap, p_reap;
} TCell;

/* Real definition of the opaque TWorld from tierra.h. */
struct TWorld {
    TConfig cfg;
    TSoup   soup;
    TRng    rng;

    /* Fixed-capacity cell pool, sized so it can never need to grow (every
     * live cell occupies >= min_cell_size soup bytes, so num_cells can
     * never exceed soup_size/min_cell_size). free_cells is a stack of
     * unused indices. Fixed capacity matters: it lets cpu_step hold a
     * TCpu* across a divide() that allocates a new cell without that
     * pointer being invalidated by a realloc. */
    TCell  *cells;
    int32_t cells_cap;
    int32_t num_cells;

    int32_t *free_cells;
    int32_t  free_n;

    int32_t ce;                    /* current cell index (slicer cursor), -1 if empty */
    int32_t top_reap, bottom_reap; /* reaper queue ends, -1 if empty */

    int64_t sum_mm_size;  /* running sum of live cells' mm_size, for avg_size */

    double   avg_repinst; /* running estimate of legacy RepInst, for lazy reaping */
    uint64_t num_divides;

    /* Mutation rate state (mutate.c), recomputed every RATE_RECALC_INTERVAL
     * instructions from avg_repinst/avg_size/num_cells (CalcFlawRates). */
    int32_t rate_mut, rate_flaw, rate_mov_mut;
    int32_t count_mut_rate, count_flaw, count_mov_mut;
    uint64_t next_rate_calc;

    TGenebank gb;

    uint64_t inst_executed;
    uint64_t births, deaths;
    uint64_t mutations, flaws;

    time_t start_time; /* for TStats::speed */

    TStats stats; /* scratch buffer returned by t_stats */

    /* lazily-built view buffers for t_cells/t_genotypes */
    TCellView *cell_view;
    int32_t    cell_view_cap;
    TGenoView *geno_view;
    int32_t    geno_view_cap;
};

/* Per-cell context passed as TExecCtx::user while cell `idx` is executing. */
typedef struct TCellCtx {
    TWorld *w;
    int32_t idx;
} TCellCtx;

/* Pop a free cell slot (zeroed, genotype=-1, alive=0, queue links set to
 * "not in any queue"). The pool's fixed capacity guarantees this never
 * fails -- see the comment on TWorld::cells above. */
int32_t world_get_free_cell(TWorld *w);

#endif /* TIERRA_WORLD_H */
