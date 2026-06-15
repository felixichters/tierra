#ifndef TIERRA_CPU_H
#define TIERRA_CPU_H

#include <stdint.h>
#include "soup.h"

#define TCPU_STACK_SIZE 10
#define TCPU_NUM_REGS   4  /* AX BX CX DX */

/* MaxMalMult (tierra/soup_in): max growth factor for a mal() request or a
 * genetic operator's reallocation, relative to the requesting cell's own
 * current mm_size. Bounds both exec_mal's request-size check (here) and
 * splice_genome's daughter-resize check (world.h's MOV_PROP_THR_DIV etc). */
#define MAX_MAL_MULT 3.0

typedef struct TFlags {
    int e; /* error */
    int s; /* sign (result < 0) */
    int z; /* zero (result == 0) */
} TFlags;

/* A single CPU. One per cell; the legacy multi-CPU-per-cell (SHADOW/PLOIDY)
 * machinery is dropped per the plan. */
typedef struct TCpu {
    int32_t ax, bx, cx, dx;
    int32_t sp;
    int32_t ip;
    TFlags  fl;
    int32_t stack[TCPU_STACK_SIZE];
} TCpu;

/* Everything an instruction needs beyond the CPU's own registers.
 * `soup` is mandatory; the rest are optional extension points wired up by
 * mutate.c (step 4) and world.c (step 3). With all left NULL, the CPU runs
 * deterministically and `mal`/`divide` always fail (a lone CPU outside a
 * TWorld has no cell record to allocate for or divide into). */
typedef struct TExecCtx {
    TSoup *soup;

    /* Bound on how many candidate positions adr/adrb/adrf/jmp/jmpb/call
     * scan per direction before giving up. world.c derives this from
     * cfg->search_limit * avg_cell_size each tick. */
    int32_t search_limit;
    int32_t min_templ_size;

    /* The executing cell's own mm_size, in bytes. exec_mal rejects requests
     * larger than MAX_MAL_MULT * own_mm_size (mirroring legacy mal()'s
     * `sug_size > MaxMalMult * ce->mm.s` guard). 0 means mal() always fails
     * the size check (no cell record to bound against). */
    int32_t own_mm_size;

    /* Returns a small perturbation applied to a decoded value (0 if no
     * mutation fires). NULL means "no flaws", i.e. always 0. */
    int32_t (*flaw)(void *user);
    void *flaw_user;

    /* Opaque per-cell context shared by divide/on_mal/reap_for_space/on_mov
     * below (world.c uses it to identify which TCell is executing). */
    void *user;

    /* Attempt cell division for the `divide` instruction. mode/eject mirror
     * the legacy is.mode/is.sval at the divide() callsite. Returns 0 on
     * success (CPU flags cleared) or nonzero on failure (E flag set).
     * NULL means division is unsupported in this context. */
    int (*divide)(void *user, TCpu *cpu, int mode, int eject);

    /* Called after `mal` successfully allocates `size` instructions at
     * `addr`, so the cell record can remember its pending daughter block
     * (ce->md in the legacy engine). NULL means mal() never succeeds. */
    void (*on_mal)(void *user, int32_t addr, int32_t size);

    /* Called when `mal` finds the soup full: try to free space by reaping
     * a cell. Returns 0 if a cell was reaped (mal retries the allocation),
     * nonzero if nothing could be reaped (mal fails, E=1). NULL means mal()
     * never retries -- a full soup is a hard failure. */
    int (*reap_for_space)(void *user);

    /* Called after a successful movii write, with dst = ad(AX), the soup
     * address written. Lets world.c track daughter-memory copy bookkeeping
     * (MovOffMin/Max, mov_daught) that divide() consumes. NULL disables the
     * tracking (divide() will then always see an empty copied region). */
    void (*on_mov)(void *user, int32_t dst_addr);
} TExecCtx;

/* Zero every register/flag/stack slot and set the instruction pointer. */
void cpu_reset(TCpu *cpu, int32_t ip);

/* Fetch, decode and execute the instruction at cpu->ip (wrapped into the
 * soup), advance the instruction pointer accordingly, and return the
 * executed opcode (0-31). */
int cpu_step(TCpu *cpu, TExecCtx *ctx);

#endif /* TIERRA_CPU_H */
