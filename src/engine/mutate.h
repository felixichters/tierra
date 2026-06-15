#ifndef TIERRA_MUTATE_H
#define TIERRA_MUTATE_H

#include "world.h"

/* How often (in instructions executed, globally) mutate_recalc_rates is
 * called -- legacy CalcFlawRates runs once per million instructions. */
#define MUTATE_RATE_RECALC_INTERVAL 1000000

/* Recompute rate_mut/rate_flaw/rate_mov_mut from the current avg_repinst,
 * avg genome size and population (legacy CalcFlawRates). Called from
 * world_tick every RATE_RECALC_INTERVAL instructions. */
void mutate_recalc_rates(TWorld *w);

/* One "cosmic ray": with probability driven by rate_mut, corrupt a single
 * uniformly-random byte anywhere in the soup (legacy mutate()). Called once
 * per instruction executed. */
void mutate_cosmic_ray(TWorld *w);

/* TExecCtx::flaw callback (legacy flaw()): with probability driven by
 * rate_flaw, returns +1 or -1 (else 0). `user` is a TCellCtx*. */
int32_t mutate_flaw_cb(void *user);

/* Called after movii writes soup byte `dst_addr`: with probability driven
 * by rate_mov_mut, corrupt that byte (legacy's copy-mutation in movii). */
void mutate_movii_copy(TWorld *w, int32_t dst_addr);

/* Run the genetic operators (legacy GeneticOps(), called once per
 * successful divide before the daughter's genotype is registered):
 * point mutation, same-size crossover, instruction insertion/deletion/
 * crossover, segment insertion/deletion/crossover. May reallocate the
 * daughter cell `nc_idx`'s memory block (updating mm_addr/mm_size/mg_off/
 * mg_size/cpu.ip in place). `ce_idx` is the mother, for context only. */
void mutate_on_divide(TWorld *w, int32_t ce_idx, int32_t nc_idx);

#endif /* TIERRA_MUTATE_H */
