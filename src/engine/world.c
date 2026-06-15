#include "world.h"
#include "slicer.h"
#include "reaper.h"
#include "mutate.h"
#include "assemble.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

/* ---- cell pool ----------------------------------------------------------- */

int32_t world_get_free_cell(TWorld *w) {
    int32_t idx = w->free_cells[--w->free_n];

    TCell *c = &w->cells[idx];
    memset(c, 0, sizeof(*c));
    c->genotype = -1;
    c->n_time = c->p_time = idx;
    c->n_reap = c->p_reap = -1;
    return idx;
}

/* ---- TExecCtx callbacks ---------------------------------------------------
 * Bound to a TCellCtx{w, idx} for whichever cell is currently executing.
 */

static int world_do_divide(TWorld *w, int32_t ce_idx);

static int world_divide_cb(void *user, TCpu *cpu, int mode, int eject) {
    (void)cpu;
    (void)mode;
    (void)eject;
    TCellCtx *cc = user;
    return world_do_divide(cc->w, cc->idx);
}

static void world_on_mal(void *user, int32_t addr, int32_t size) {
    TCellCtx *cc = user;
    TCell *c = &cc->w->cells[cc->idx];

    /* A second mal() before the first daughter block was consumed by
     * divide() discards the earlier allocation (legacy mal() zeroes
     * ce->md.s when reallocating). */
    if (c->md_size > 0)
        soup_free(&cc->w->soup, c->md_addr, c->md_size);

    c->md_addr = addr;
    c->md_size = size;

    /* A fresh daughter block starts with no movii-copy progress; offsets
     * recorded against a previous (now-freed) md block would otherwise be
     * carried over and could exceed the new block's size. */
    c->mov_off_min = 0;
    c->mov_off_max = 0;
    c->mov_daught = 0;

    reaper_down_if(cc->w, cc->idx);
}

static int world_reap_for_space(void *user) {
    TCellCtx *cc = user;
    return reaper_reap(cc->w, 1, cc->idx);
}

static void world_on_mov(void *user, int32_t dst_addr) {
    TCellCtx *cc = user;
    TCell *c = &cc->w->cells[cc->idx];

    mutate_movii_copy(cc->w, dst_addr);

    if (c->md_size <= 0)
        return;

    int32_t off = dst_addr - c->md_addr;
    if (off < 0 || off >= c->md_size)
        return;

    if (off < c->mov_off_min)
        c->mov_off_min = off;
    if (off > c->mov_off_max)
        c->mov_off_max = off;
    c->mov_daught++;
}

/* ---- divide() (mode 2 / eject 0; the only mode a single-CPU cell uses) --- */

static int world_do_divide(TWorld *w, int32_t ce_idx) {
    TCell *ce = &w->cells[ce_idx];

    if (ce->md_size <= 0 || ce->md_size < w->cfg.min_cell_size)
        return -1;

    int32_t dgen = ce->mov_off_max - ce->mov_off_min + 1;
    double thresh = (double)ce->md_size * MOV_PROP_THR_DIV;

    if (dgen < w->cfg.min_genome_size || (double)dgen < thresh || (double)ce->mov_daught < thresh)
        return -1;

    int32_t nc_idx = world_get_free_cell(w);
    ce = &w->cells[ce_idx]; /* world_get_free_cell zeroes slots; re-fetch for safety */
    TCell *nc = &w->cells[nc_idx];

    nc->mm_addr = ce->md_addr;
    nc->mm_size = ce->md_size;
    nc->mg_off = ce->mov_off_min;
    nc->mg_size = dgen;

    cpu_reset(&nc->cpu, nc->mm_addr);
    nc->cpu.ax = ce->cpu.ax;
    nc->cpu.bx = ce->cpu.bx;
    nc->cpu.cx = ce->cpu.cx;
    nc->cpu.dx = ce->cpu.dx;

    nc->generation = ce->generation + 1;
    nc->alive = 1;

    /* Point mutation, crossover, insertion, deletion -- may reallocate nc's
     * memory block, updating mm_addr/mm_size/mg_off/mg_size/cpu.ip. */
    mutate_on_divide(w, ce_idx, nc_idx);

    /* Linearize the (possibly soup-wrapping) genome for the genebank. */
    uint8_t *genome = malloc((size_t)nc->mm_size);
    for (int32_t i = 0; i < nc->mm_size; i++)
        genome[i] = w->soup.mem[tierra_ad(w->soup.size, nc->mm_addr + i)];
    nc->genotype = genome_register(&w->gb, genome, nc->mm_size, nc->mg_off, nc->mg_size,
                                    ce->genotype, w->inst_executed);
    free(genome);

    slicer_insert_before(w, nc_idx, ce_idx);
    reaper_insert_bottom(w, nc_idx);

    w->num_cells++;
    w->sum_mm_size += nc->mm_size;
    w->births++;

    /* Running estimate of legacy RepInst (avg instructions per divide),
     * for reaper_lazy_cull. */
    if (w->num_divides == 0)
        w->avg_repinst = (double)ce->repinst;
    else
        w->avg_repinst = w->avg_repinst * 0.99 + (double)ce->repinst * 0.01;
    w->num_divides++;

    /* Mother: daughter block consumed, reproduction-cycle counters reset. */
    ce->md_addr = 0;
    ce->md_size = 0;
    ce->fecundity++;
    ce->repinst = 0;
    ce->mov_daught = 0;
    ce->mov_off_min = 0;
    ce->mov_off_max = 0;

    reaper_down_if(w, ce_idx);

    return 0;
}

/* ---- main loop ------------------------------------------------------------- */

static void world_tick(TWorld *w) {
    if (w->ce < 0)
        return;

    int32_t cell_idx = w->ce;
    TCell *c = &w->cells[cell_idx];

    c->ib += slicer_slice_size(w, c);
    int64_t budget = c->ib;

    double avg_size = (w->num_cells > 0)
        ? (double)w->sum_mm_size / (double)w->num_cells
        : (double)w->cfg.min_cell_size;
    int32_t search_limit = (int32_t)(w->cfg.search_limit * avg_size);
    if (search_limit < 1)
        search_limit = 1;

    TCellCtx cc = { w, cell_idx };
    TExecCtx ctx = {0};
    ctx.soup = &w->soup;
    ctx.search_limit = search_limit;
    ctx.min_templ_size = w->cfg.min_templ_size;
    ctx.own_mm_size = c->mm_size;
    ctx.user = &cc;
    ctx.divide = world_divide_cb;
    ctx.on_mal = world_on_mal;
    ctx.reap_for_space = world_reap_for_space;
    ctx.on_mov = world_on_mov;
    ctx.flaw = mutate_flaw_cb;
    ctx.flaw_user = &cc;

    while (budget > 0) {
        if (w->inst_executed >= w->next_rate_calc) {
            mutate_recalc_rates(w);
            w->next_rate_calc += MUTATE_RATE_RECALC_INTERVAL;
        }
        mutate_cosmic_ray(w);

        cpu_step(&c->cpu, &ctx);

        if (c->cpu.fl.e) {
            c->err_flags++;
            reaper_up_if(w, cell_idx);
        }
        c->inst++;
        c->repinst++;
        w->inst_executed++;
        budget--;
    }
    c->ib = budget;

    slicer_advance(w);
    reaper_lazy_cull(w);
}

/* ---- lifecycle -------------------------------------------------------------- */

TWorld *t_create(const TConfig *cfg) {
    TWorld *w = calloc(1, sizeof(*w));

    w->cfg = *cfg;
    soup_init(&w->soup, cfg);
    rng_seed(&w->rng, cfg->seed ? cfg->seed : (uint64_t)time(NULL));

    /* See the comment on TWorld::cells in world.h for why this bound is
     * exact and safe: every live cell occupies >= min_cell_size bytes. */
    w->cells_cap = cfg->soup_size / cfg->min_cell_size + 2;
    w->cells = calloc((size_t)w->cells_cap, sizeof(TCell));
    w->free_cells = malloc((size_t)w->cells_cap * sizeof(int32_t));
    for (int32_t i = 0; i < w->cells_cap; i++)
        w->free_cells[i] = w->cells_cap - 1 - i;
    w->free_n = w->cells_cap;

    w->ce = -1;
    w->top_reap = w->bottom_reap = -1;

    genome_init(&w->gb, cfg->soup_size);

    w->start_time = time(NULL);

    return w;
}

void t_destroy(TWorld *w) {
    if (!w)
        return;
    soup_destroy(&w->soup);
    genome_destroy(&w->gb);
    free(w->cells);
    free(w->free_cells);
    free(w->cell_view);
    free(w->geno_view);
    free(w);
}

int t_seed_file(TWorld *w, const char *tie_path) {
    /* Parse into a same-sized scratch soup at address 0 to discover the
     * genome length, then copy that many bytes into a real allocation. */
    TSoup scratch;
    soup_init(&scratch, &w->cfg);

    int32_t len = assemble_load_tie(&scratch, tie_path, 0);
    if (len < 0) {
        soup_destroy(&scratch);
        return -1;
    }

    int32_t addr = soup_alloc(&w->soup, len, 0);
    if (addr < 0) {
        soup_destroy(&scratch);
        return -1;
    }
    for (int32_t i = 0; i < len; i++)
        w->soup.mem[tierra_ad(w->soup.size, addr + i)] = scratch.mem[i];
    soup_destroy(&scratch);

    int32_t idx = world_get_free_cell(w);
    TCell *c = &w->cells[idx];
    c->mm_addr = addr;
    c->mm_size = len;
    c->mg_off = 0;
    c->mg_size = len;
    cpu_reset(&c->cpu, addr);
    c->alive = 1;

    uint8_t *genome = malloc((size_t)len);
    for (int32_t i = 0; i < len; i++)
        genome[i] = w->soup.mem[tierra_ad(w->soup.size, addr + i)];
    c->genotype = genome_register(&w->gb, genome, len, 0, len, -1, 0);
    free(genome);

    /* world_get_free_cell already left c's queue links as a valid singleton
     * (n_time=p_time=idx, n_reap=p_reap=-1); just point the world at it. */
    w->ce = idx;
    w->top_reap = w->bottom_reap = idx;
    w->num_cells = 1;
    w->sum_mm_size = len;
    w->births = 1;
    return 0;
}

uint64_t t_step(TWorld *w, uint64_t max_inst) {
    uint64_t start = w->inst_executed;
    while (w->inst_executed - start < max_inst && w->ce >= 0)
        world_tick(w);
    return w->inst_executed - start;
}

/* ---- telemetry --------------------------------------------------------------- */

const TStats *t_stats(const TWorld *cw) {
    TWorld *w = (TWorld *)cw;
    TStats *st = &w->stats;
    memset(st, 0, sizeof(*st));

    st->inst_executed = w->inst_executed;
    st->num_cells = w->num_cells;
    st->births = w->births;
    st->deaths = w->deaths;
    st->mutations = w->mutations;
    st->flaws = w->flaws;

    if (w->num_cells > 0) {
        st->avg_size = (double)w->sum_mm_size / (double)w->num_cells;

        uint64_t gen_sum = 0, fec_sum = 0;
        for (int32_t i = 0; i < w->cells_cap; i++) {
            if (!w->cells[i].alive)
                continue;
            gen_sum += (uint64_t)w->cells[i].generation;
            fec_sum += (uint64_t)w->cells[i].fecundity;
        }
        st->generations = (double)gen_sum / (double)w->num_cells;
        st->avg_fecundity = (double)fec_sum / (double)w->num_cells;
    }

    uint8_t *seen_size = calloc((size_t)w->gb.max_size + 1, 1);
    for (int32_t i = 0; i < w->gb.all_n; i++) {
        const TGenotype *g = w->gb.all[i];
        if (g->pop <= 0)
            continue;
        st->num_genotypes++;
        if (!seen_size[g->size]) {
            seen_size[g->size] = 1;
            st->num_sizes++;
        }
        if (g->pop > st->max_pop) {
            st->max_pop = g->pop;
            st->max_pop_genotype = i;
        }
    }
    free(seen_size);

    st->mem_occupancy = 1.0 - (double)soup_free_total(&w->soup) / (double)w->soup.size;

    time_t elapsed = time(NULL) - w->start_time;
    st->speed = (elapsed > 0) ? (double)w->inst_executed / (double)elapsed : 0.0;

    return st;
}

int t_set_param(TWorld *w, TParam p, double v) {
    switch (p) {
    case T_PARAM_SLICE_SIZE:      w->cfg.slice_size = (int32_t)v; return 0;
    case T_PARAM_GEN_PER_BKG_MUT: w->cfg.gen_per_bkg_mut = (int32_t)v; return 0;
    case T_PARAM_GEN_PER_FLAW:    w->cfg.gen_per_flaw = (int32_t)v; return 0;
    case T_PARAM_GEN_PER_MOV_MUT: w->cfg.gen_per_mov_mut = (int32_t)v; return 0;
    case T_PARAM_GEN_PER_DIV_MUT: w->cfg.gen_per_div_mut = (int32_t)v; return 0;
    case T_PARAM_DIST_FREQ:       w->cfg.dist_freq = v; return 0;
    case T_PARAM_DIST_PROP:       w->cfg.dist_prop = v; return 0;
    case T_PARAM_REAP_RND_PROP:   w->cfg.reap_rnd_prop = v; return 0;
    case T_PARAM_MAL_MODE:
        w->cfg.mal_mode = (int32_t)v;
        w->soup.mal_mode = (int32_t)v;
        return 0;
    default:
        return -1;
    }
}

double t_get_param(const TWorld *w, TParam p) {
    switch (p) {
    case T_PARAM_SLICE_SIZE:      return w->cfg.slice_size;
    case T_PARAM_GEN_PER_BKG_MUT: return w->cfg.gen_per_bkg_mut;
    case T_PARAM_GEN_PER_FLAW:    return w->cfg.gen_per_flaw;
    case T_PARAM_GEN_PER_MOV_MUT: return w->cfg.gen_per_mov_mut;
    case T_PARAM_GEN_PER_DIV_MUT: return w->cfg.gen_per_div_mut;
    case T_PARAM_DIST_FREQ:       return w->cfg.dist_freq;
    case T_PARAM_DIST_PROP:       return w->cfg.dist_prop;
    case T_PARAM_REAP_RND_PROP:   return w->cfg.reap_rnd_prop;
    case T_PARAM_MAL_MODE:        return w->cfg.mal_mode;
    default:                      return 0.0;
    }
}

/* ---- read-only views ---------------------------------------------------------- */

const uint8_t *t_soup(const TWorld *w, int *size) {
    if (size)
        *size = w->soup.size;
    return w->soup.mem;
}

const TCellView *t_cells(const TWorld *cw, int *n) {
    TWorld *w = (TWorld *)cw;

    if (w->cell_view_cap < w->num_cells) {
        free(w->cell_view);
        w->cell_view_cap = w->num_cells;
        w->cell_view = malloc((size_t)w->cell_view_cap * sizeof(TCellView));
    }

    int32_t count = 0;
    for (int32_t i = 0; i < w->cells_cap; i++) {
        if (!w->cells[i].alive)
            continue;
        const TCell *c = &w->cells[i];
        TCellView *v = &w->cell_view[count++];
        v->addr = c->mm_addr;
        v->size = c->mm_size;
        v->ip = c->cpu.ip;
        v->genotype_id = c->genotype;
        v->age = c->inst;
        v->alive = 1;
    }
    if (n)
        *n = count;
    return w->cell_view;
}

/* Stable per-genotype colour, 0xRRGGBBAA with alpha forced opaque. */
static uint32_t genotype_colour(int32_t id) {
    uint32_t x = (uint32_t)id * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return (x & 0xFFFFFF00u) | 0xFFu;
}

const TGenoView *t_genotypes(const TWorld *cw, int *n) {
    TWorld *w = (TWorld *)cw;

    int32_t count = 0;
    for (int32_t i = 0; i < w->gb.all_n; i++)
        if (w->gb.all[i]->pop > 0)
            count++;

    if (w->geno_view_cap < count) {
        free(w->geno_view);
        w->geno_view_cap = count;
        w->geno_view = malloc((size_t)w->geno_view_cap * sizeof(TGenoView));
    }

    int32_t out = 0;
    for (int32_t i = 0; i < w->gb.all_n; i++) {
        const TGenotype *g = w->gb.all[i];
        if (g->pop <= 0)
            continue;
        TGenoView *v = &w->geno_view[out++];
        v->id = i;
        genome_name(&w->gb, i, v->name);
        v->size = g->size;
        v->pop = g->pop;
        v->colour = genotype_colour(i);
    }
    if (n)
        *n = out;
    return w->geno_view;
}
