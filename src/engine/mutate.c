/* Mutation operators, grounded in Tierra 6.02's operator.c/instruct.c/
 * bookeep.c (mutate(), flaw(), mut_site(), MutationOps(), GeneticOps() and
 * its 7 insertion/deletion/crossover operators, CalcFlawRates()).
 *
 * PLOIDY==1 throughout (single track); NET/Beagle, disk-genebank and
 * thread-analysis bookkeeping hooks are dropped per the plan's non-goals.
 */

#include "mutate.h"
#include "isa.h"
#include "util.h"

#include <stdlib.h>

/* ---- shared primitives ----------------------------------------------------- */

static uint8_t genome_byte(const TWorld *w, int32_t addr, int32_t off) {
    return w->soup.mem[tierra_ad(w->soup.size, addr + off)];
}

static void set_genome_byte(TWorld *w, int32_t addr, int32_t off, uint8_t v) {
    w->soup.mem[tierra_ad(w->soup.size, addr + off)] = v;
}

/* mut_site(): 20% chance to flip one of the low 5 (opcode) bits, else
 * replace the byte with a uniformly random opcode 0..31. */
static void mut_site(TWorld *w, int32_t addr) {
    int32_t a = tierra_ad(w->soup.size, addr);
    if (rng_double(&w->rng) < w->cfg.mut_bit_prop)
        w->soup.mem[a] ^= (uint8_t)(1u << rng_below(&w->rng, 5));
    else
        w->soup.mem[a] = (uint8_t)rng_below(&w->rng, TIERRA_NUM_OPS);
    w->mutations++;
}

static int is_nop_byte(const TWorld *w, int32_t addr, int32_t off) {
    uint8_t op = genome_byte(w, addr, off) & 0x1F;
    return op == OP_NOP0 || op == OP_NOP1;
}

/* ---- rate recalculation (legacy CalcFlawRates, every 1M instructions) ----- */

void mutate_recalc_rates(TWorld *w) {
    double avg_size = (w->num_cells > 0)
        ? (double)w->sum_mm_size / (double)w->num_cells
        : (double)w->cfg.min_cell_size;

    double rep_inst, pop_gen_time;
    if (w->num_divides == 0) {
        rep_inst = 10.0 * avg_size;
        pop_gen_time = (avg_size > 0.0)
            ? rep_inst * ((double)w->cfg.soup_size / (4.0 * avg_size))
            : 0.0;
    } else {
        rep_inst = w->avg_repinst;
        pop_gen_time = (double)w->num_cells * rep_inst;
    }

    double prob_of_hit = avg_size / (double)w->cfg.soup_size;

    w->rate_mut = (w->cfg.gen_per_bkg_mut > 0)
        ? (int32_t)(pop_gen_time * 2.0 * (double)w->cfg.gen_per_bkg_mut * prob_of_hit)
        : 0;
    w->rate_flaw = (w->cfg.gen_per_flaw > 0)
        ? (int32_t)(rep_inst * (double)w->cfg.gen_per_flaw * 2.0)
        : 0;
    w->rate_mov_mut = (w->cfg.gen_per_mov_mut > 0)
        ? (int32_t)(2.0 * (double)w->cfg.gen_per_mov_mut * avg_size)
        : 0;

    if (w->rate_mut < 0)     w->rate_mut = 0;
    if (w->rate_flaw < 0)    w->rate_flaw = 0;
    if (w->rate_mov_mut < 0) w->rate_mov_mut = 0;
}

/* ---- per-instruction mechanisms -------------------------------------------- */

void mutate_cosmic_ray(TWorld *w) {
    if (!w->rate_mut)
        return;
    if (++w->count_mut_rate >= w->rate_mut) {
        int32_t addr = (int32_t)rng_below(&w->rng, (uint32_t)w->soup.size);
        mut_site(w, addr);
        w->count_mut_rate = (int32_t)rng_below(&w->rng, (uint32_t)w->rate_mut);
    }
}

int32_t mutate_flaw_cb(void *user) {
    TCellCtx *cc = user;
    TWorld *w = cc->w;

    if (!w->rate_flaw)
        return 0;
    if (++w->count_flaw >= w->rate_flaw) {
        w->count_flaw = (int32_t)rng_below(&w->rng, (uint32_t)w->rate_flaw);
        w->flaws++;
        return rng_below(&w->rng, 2) ? -1 : 1;
    }
    return 0;
}

void mutate_movii_copy(TWorld *w, int32_t dst_addr) {
    if (!w->rate_mov_mut)
        return;
    if (++w->count_mov_mut >= w->rate_mov_mut) {
        mut_site(w, dst_addr);
        w->count_mov_mut = (int32_t)rng_below(&w->rng, (uint32_t)w->rate_mov_mut);
    }
}

/* ---- divide-time genetic operators (legacy GeneticOps) --------------------- */

/* MutationOps: GenPerDivMut, a flat 1/GenPerDivMut chance per divide (the
 * `while` lets it fire more than once, vanishingly rarely), of flipping one
 * byte within the daughter's genetic memory. */
static void op_point_mutation(TWorld *w, int32_t nc_idx) {
    while (w->cfg.gen_per_div_mut > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_div_mut) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t site = nc->mg_off + (int32_t)rng_below(&w->rng, (uint32_t)nc->mg_size);
        mut_site(w, nc->mm_addr + site);
    }
}

/* Uniformly random live cell, reached by walking the slicer queue a random
 * number of steps (0..num_cells-1) from `from` -- may return `from` itself
 * (legacy RandomCell). */
static int32_t random_cell(TWorld *w, int32_t from) {
    int32_t steps = (int32_t)rng_below(&w->rng, (uint32_t)w->num_cells);
    int32_t idx = from;
    for (int32_t i = 0; i < steps; i++)
        idx = w->cells[idx].n_time;
    return idx;
}

/* First live cell other than `from`, walking the slicer queue, whose
 * genetic-memory size is within `tol` of `size`; -1 if none (legacy
 * FindRandCellOfSize). */
static int32_t find_cell_of_size(TWorld *w, int32_t from, int32_t size, int32_t tol) {
    int32_t idx = w->cells[from].n_time;
    while (idx != from) {
        int32_t sz = w->cells[idx].mg_size;
        if (sz >= size - tol && sz <= size + tol)
            return idx;
        idx = w->cells[idx].n_time;
    }
    return -1;
}

/* A byte range to splice: `size` bytes starting at absolute soup address
 * `addr` (wrapped via ad()). */
typedef struct GFrag {
    int32_t addr;
    int32_t size;
} GFrag;

/* SharedGenOps: assemble up to 3 fragments into a new genome for cell
 * `nc_idx`, gate on viability (MinCellSize/MinGenMemSiz/MovPropThrDiv/
 * MaxMalMult), and either overwrite the existing genome in place (size
 * unchanged) or reallocate the cell's memory block (size changed, updating
 * mm_addr/mm_size/mg_off/mg_size/cpu.ip). Returns 1 on success, 0 if the
 * splice was rejected and the daughter is left unchanged. */
static int splice_genome(TWorld *w, int32_t nc_idx, const GFrag *frags, int nfrags) {
    TCell *nc = &w->cells[nc_idx];

    int32_t new_gen_size = 0;
    for (int i = 0; i < nfrags; i++)
        new_gen_size += frags[i].size;

    int32_t data_size = nc->mm_size - nc->mg_size;
    int32_t new_cell_size = new_gen_size + data_size;
    double thresh = (double)new_cell_size * MOV_PROP_THR_DIV;

    if ((double)new_cell_size > MAX_MAL_MULT * (double)nc->mm_size ||
        new_cell_size < w->cfg.min_cell_size ||
        new_gen_size < w->cfg.min_genome_size ||
        (double)new_gen_size < thresh)
        return 0;

    uint8_t *buf = malloc((size_t)new_gen_size);
    int32_t off = 0;
    for (int i = 0; i < nfrags; i++) {
        for (int32_t j = 0; j < frags[i].size; j++)
            buf[off + j] = genome_byte(w, frags[i].addr, j);
        off += frags[i].size;
    }

    if (new_gen_size == nc->mg_size) {
        for (int32_t i = 0; i < new_gen_size; i++)
            set_genome_byte(w, nc->mm_addr + nc->mg_off, i, buf[i]);
    } else {
        int32_t new_addr = soup_alloc(&w->soup, new_cell_size, nc->mm_addr);
        if (new_addr < 0) {
            free(buf);
            return 0;
        }
        for (int32_t i = 0; i < new_gen_size; i++)
            w->soup.mem[tierra_ad(w->soup.size, new_addr + i)] = buf[i];
        soup_free(&w->soup, nc->mm_addr, nc->mm_size);

        /* w->sum_mm_size is updated by world_do_divide using nc's *final*
         * mm_size after all GeneticOps run -- nc's pre-mutation size was
         * never added, so don't touch sum_mm_size here. */
        nc->mm_addr = new_addr;
        nc->mm_size = new_cell_size;
        nc->mg_off = 0;
        nc->mg_size = new_gen_size;
        nc->cpu.ip = new_addr;
    }

    free(buf);
    return 1;
}

/* CrossoverInstSamSiz: GenPerCroInsSamSiz, splice a same-size chunk from a
 * similarly-sized mate's genome across a random crossover point. No size
 * change, so written directly without going through splice_genome. */
static void op_crossover_same_size(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    while (w->cfg.gen_per_cro_ins_sam_siz > 0 &&
           rng_below(&w->rng, (uint32_t)w->cfg.gen_per_cro_ins_sam_siz) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t mate = find_cell_of_size(w, ce_idx, nc->mg_size, MATE_SIZE_EP);
        if (mate < 0)
            return;

        TCell *mc = &w->cells[mate];
        int32_t size = (nc->mg_size < mc->mg_size) ? nc->mg_size : mc->mg_size;
        if (size < 2)
            return;

        int32_t cross = 1 + (int32_t)rng_below(&w->rng, (uint32_t)(size - 1));
        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t m_addr = mc->mm_addr + mc->mg_off;

        if (2 * cross > nc->mg_size) {
            for (int32_t i = cross; i < size; i++)
                set_genome_byte(w, d_addr, i, genome_byte(w, m_addr, i));
        } else {
            for (int32_t i = 0; i < cross; i++)
                set_genome_byte(w, d_addr, i, genome_byte(w, m_addr, i));
        }
        w->mutations++;
    }
}

/* CrossoverInst: GenPerCroIns, splice [0,DCross) of the daughter with
 * [MCross,end) of a random mate (or vice versa, whichever keeps the larger
 * half on the side past its crossover point). Failure aborts further
 * attempts this divide (legacy: `return` on SharedGenOps failure). */
static void op_crossover_inst(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    while (w->cfg.gen_per_cro_ins > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_cro_ins) == 0) {
        TCell *nc = &w->cells[nc_idx];
        if (nc->mg_size < 2)
            return;
        int32_t d_cross = 1 + (int32_t)rng_below(&w->rng, (uint32_t)(nc->mg_size - 1));

        int32_t mate = random_cell(w, ce_idx);
        TCell *mc = &w->cells[mate];
        if (mc->mg_size < 2)
            return;
        int32_t m_cross = 1 + (int32_t)rng_below(&w->rng, (uint32_t)(mc->mg_size - 1));

        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t m_addr = mc->mm_addr + mc->mg_off;
        int32_t d_size = nc->mg_size, m_size = mc->mg_size;

        GFrag frags[2];
        if (2 * d_cross > d_size) {
            frags[0] = (GFrag){d_addr, d_cross};
            frags[1] = (GFrag){m_addr + m_cross, m_size - m_cross};
        } else {
            frags[0] = (GFrag){m_addr, m_cross};
            frags[1] = (GFrag){d_addr + d_cross, d_size - d_cross};
        }

        if (!splice_genome(w, nc_idx, frags, 2))
            return;
        w->mutations++;
    }
}

/* InsertionInst: GenPerInsIns, splice a random contiguous chunk of a random
 * mate's genome into the daughter at a random instruction boundary.
 * Failure aborts further attempts (legacy: `return`). */
static void op_insertion_inst(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    while (w->cfg.gen_per_ins_ins > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_ins_ins) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t d_size = nc->mg_size;
        int32_t ins_off = (int32_t)rng_below(&w->rng, (uint32_t)(d_size + 1));

        int32_t mate = random_cell(w, ce_idx);
        TCell *mc = &w->cells[mate];
        if (mc->mg_size < 1)
            return;
        int32_t chunk_size = 1 + (int32_t)rng_below(&w->rng, (uint32_t)mc->mg_size);
        int32_t chunk_start = (int32_t)rng_below(&w->rng, (uint32_t)(mc->mg_size - chunk_size + 1));

        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t m_addr = mc->mm_addr + mc->mg_off + chunk_start;

        GFrag frags[3];
        int n;
        if (ins_off == 0) {
            frags[0] = (GFrag){m_addr, chunk_size};
            frags[1] = (GFrag){d_addr, d_size};
            n = 2;
        } else if (ins_off == d_size) {
            frags[0] = (GFrag){d_addr, d_size};
            frags[1] = (GFrag){m_addr, chunk_size};
            n = 2;
        } else {
            frags[0] = (GFrag){d_addr, ins_off};
            frags[1] = (GFrag){m_addr, chunk_size};
            frags[2] = (GFrag){d_addr + ins_off, d_size - ins_off};
            n = 3;
        }

        if (!splice_genome(w, nc_idx, frags, n))
            return;
        w->mutations++;
    }
}

/* DeletionInst: GenPerDelIns, delete a random run of up to half the
 * daughter's genome. Failure just retries (legacy: loop continues). */
static void op_deletion_inst(TWorld *w, int32_t nc_idx) {
    while (w->cfg.gen_per_del_ins > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_del_ins) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t d_size = nc->mg_size;
        if (d_size < 2)
            return;
        int32_t del_size = 1 + (int32_t)rng_below(&w->rng, (uint32_t)(d_size / 2));
        int32_t del_off = (int32_t)rng_below(&w->rng, (uint32_t)(d_size - del_size + 1));

        int32_t d_addr = nc->mm_addr + nc->mg_off;
        GFrag frags[2];
        int n = 0;
        if (del_off > 0)
            frags[n++] = (GFrag){d_addr, del_off};
        if (del_off + del_size < d_size)
            frags[n++] = (GFrag){d_addr + del_off + del_size, d_size - del_off - del_size};
        if (n == 0)
            return;

        if (splice_genome(w, nc_idx, frags, n))
            w->mutations++;
    }
}

/* Segment boundaries within a `size`-byte genome at `addr`: a segment is a
 * (possibly empty) nop0/nop1 template run followed by a run of non-template
 * instructions. `starts` must hold at least `size` entries; returns the
 * segment count. */
static int32_t find_segments(const TWorld *w, int32_t addr, int32_t size, int32_t *starts) {
    int32_t n = 0, i = 0;
    while (i < size) {
        starts[n++] = i;
        while (i < size && is_nop_byte(w, addr, i)) i++;
        while (i < size && !is_nop_byte(w, addr, i)) i++;
    }
    return n;
}

/* DeletionSeg: GenPerDelSeg, like DeletionInst but deletes a run of whole
 * segments rather than an arbitrary byte range. Requires >=2 segments. */
static void op_deletion_seg(TWorld *w, int32_t nc_idx) {
    while (w->cfg.gen_per_del_seg > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_del_seg) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t d_size = nc->mg_size;

        int32_t *starts = malloc(sizeof(int32_t) * (size_t)d_size);
        int32_t n = find_segments(w, d_addr, d_size, starts);
        if (n < 2) { free(starts); return; }

        int32_t del_n = 1 + (int32_t)rng_below(&w->rng, (uint32_t)(n / 2));
        int32_t del_off = (int32_t)rng_below(&w->rng, (uint32_t)(n - del_n + 1));
        int32_t del_start = starts[del_off];
        int32_t del_end = (del_off + del_n < n) ? starts[del_off + del_n] : d_size;
        free(starts);

        GFrag frags[2];
        int nf = 0;
        if (del_start > 0)
            frags[nf++] = (GFrag){d_addr, del_start};
        if (del_end < d_size)
            frags[nf++] = (GFrag){d_addr + del_end, d_size - del_end};
        if (nf == 0)
            return;

        if (splice_genome(w, nc_idx, frags, nf))
            w->mutations++;
    }
}

/* InsertionSeg: GenPerInsSeg, like InsertionInst but inserts a run of whole
 * segments from a random mate at a segment boundary. Requires both the
 * daughter and the mate to have >=1 segment. Failure aborts (legacy:
 * `return`). */
static void op_insertion_seg(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    while (w->cfg.gen_per_ins_seg > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_ins_seg) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t d_size = nc->mg_size;

        int32_t *d_starts = malloc(sizeof(int32_t) * (size_t)d_size);
        int32_t d_n = find_segments(w, d_addr, d_size, d_starts);
        if (d_n < 1) { free(d_starts); return; }

        int32_t mate = random_cell(w, ce_idx);
        TCell *mc = &w->cells[mate];
        int32_t m_addr = mc->mm_addr + mc->mg_off;
        int32_t m_size = mc->mg_size;

        int32_t *m_starts = malloc(sizeof(int32_t) * (size_t)m_size);
        int32_t m_n = find_segments(w, m_addr, m_size, m_starts);
        if (m_n < 1) { free(d_starts); free(m_starts); return; }

        int32_t ins_seg = (int32_t)rng_below(&w->rng, (uint32_t)(d_n + 1));
        int32_t chunk_n = 1 + (int32_t)rng_below(&w->rng, (uint32_t)m_n);
        int32_t chunk_off = (int32_t)rng_below(&w->rng, (uint32_t)(m_n - chunk_n + 1));

        int32_t chunk_start = m_starts[chunk_off];
        int32_t chunk_end = (chunk_off + chunk_n < m_n) ? m_starts[chunk_off + chunk_n] : m_size;
        int32_t ins_off = (ins_seg < d_n) ? d_starts[ins_seg] : d_size;
        free(d_starts);
        free(m_starts);

        int32_t chunk_size = chunk_end - chunk_start;

        GFrag frags[3];
        int n;
        if (ins_off == 0) {
            frags[0] = (GFrag){m_addr + chunk_start, chunk_size};
            frags[1] = (GFrag){d_addr, d_size};
            n = 2;
        } else if (ins_off == d_size) {
            frags[0] = (GFrag){d_addr, d_size};
            frags[1] = (GFrag){m_addr + chunk_start, chunk_size};
            n = 2;
        } else {
            frags[0] = (GFrag){d_addr, ins_off};
            frags[1] = (GFrag){m_addr + chunk_start, chunk_size};
            frags[2] = (GFrag){d_addr + ins_off, d_size - ins_off};
            n = 3;
        }

        if (!splice_genome(w, nc_idx, frags, n))
            return;
        w->mutations++;
    }
}

/* CrossoverSeg: GenPerCroSeg, like CrossoverInst but the crossover points
 * fall on segment boundaries. Requires both daughter and mate to have >=2
 * segments. Failure just retries (legacy: loop continues). */
static void op_crossover_seg(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    while (w->cfg.gen_per_cro_seg > 0 && rng_below(&w->rng, (uint32_t)w->cfg.gen_per_cro_seg) == 0) {
        TCell *nc = &w->cells[nc_idx];
        int32_t d_addr = nc->mm_addr + nc->mg_off;
        int32_t d_size = nc->mg_size;

        int32_t *d_starts = malloc(sizeof(int32_t) * (size_t)d_size);
        int32_t d_n = find_segments(w, d_addr, d_size, d_starts);
        if (d_n < 2) { free(d_starts); return; }

        int32_t mate = random_cell(w, ce_idx);
        TCell *mc = &w->cells[mate];
        int32_t m_addr = mc->mm_addr + mc->mg_off;
        int32_t m_size = mc->mg_size;

        int32_t *m_starts = malloc(sizeof(int32_t) * (size_t)m_size);
        int32_t m_n = find_segments(w, m_addr, m_size, m_starts);
        if (m_n < 2) { free(d_starts); free(m_starts); return; }

        int32_t d_cross = d_starts[1 + (int32_t)rng_below(&w->rng, (uint32_t)(d_n - 1))];
        int32_t m_cross = m_starts[1 + (int32_t)rng_below(&w->rng, (uint32_t)(m_n - 1))];
        free(d_starts);
        free(m_starts);

        GFrag frags[2];
        if (2 * d_cross > d_size) {
            frags[0] = (GFrag){d_addr, d_cross};
            frags[1] = (GFrag){m_addr + m_cross, m_size - m_cross};
        } else {
            frags[0] = (GFrag){m_addr, m_cross};
            frags[1] = (GFrag){d_addr + d_cross, d_size - d_cross};
        }

        if (splice_genome(w, nc_idx, frags, 2))
            w->mutations++;
    }
}

void mutate_on_divide(TWorld *w, int32_t ce_idx, int32_t nc_idx) {
    op_point_mutation(w, nc_idx);
    op_crossover_same_size(w, ce_idx, nc_idx);
    op_crossover_inst(w, ce_idx, nc_idx);
    op_insertion_inst(w, ce_idx, nc_idx);
    op_deletion_inst(w, nc_idx);
    op_crossover_seg(w, ce_idx, nc_idx);
    op_insertion_seg(w, ce_idx, nc_idx);
    op_deletion_seg(w, nc_idx);
}
