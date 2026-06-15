/* Throwaway smoke test for step 4 (mutation operators / evolution).
 * Verifies: with default mutation rates (GenPerBkgMut=32, GenPerFlaw=32,
 * GenPerDivMut=32, indel/crossover=32), over tens of millions of
 * instructions NumGenotypes and NumSizes rise above 1 and average size
 * drifts away from the 80-instruction ancestor, while the run remains
 * deterministic for a fixed seed.
 *
 * Build: clang -std=c11 -Iinclude tests/smoke4.c -L. -ltierra -lm -o /tmp/smoke4
 * Run:   /tmp/smoke4 assets/0080aaa.tie
 */
#include <stdio.h>
#include <stdlib.h>

#include "tierra/tierra.h"

static uint64_t run_once(const char *tie_path, uint64_t seed, uint64_t max_inst,
                          int verbose, int32_t *max_genotypes, int32_t *max_sizes) {
    TConfig cfg = t_config_default();
    cfg.seed = seed;

    TWorld *w = t_create(&cfg);
    if (t_seed_file(w, tie_path) != 0) {
        fprintf(stderr, "t_seed_file failed for %s\n", tie_path);
        exit(1);
    }

    uint64_t done = 0;
    uint64_t chunk = 1000000;
    uint64_t checksum = 0;
    *max_genotypes = 0;
    *max_sizes = 0;
    while (done < max_inst) {
        uint64_t n = t_step(w, chunk);
        done += n;
        const TStats *st = t_stats(w);
        if (st->num_genotypes > *max_genotypes) *max_genotypes = st->num_genotypes;
        if (st->num_sizes > *max_sizes) *max_sizes = st->num_sizes;
        if (verbose)
            printf("  inst=%-10llu cells=%-5d genotypes=%-4d sizes=%-4d avg_size=%-8.2f "
                   "occ=%.4f mut=%llu flaws=%llu births=%llu deaths=%llu\n",
                   (unsigned long long)st->inst_executed, st->num_cells,
                   st->num_genotypes, st->num_sizes, st->avg_size, st->mem_occupancy,
                   (unsigned long long)st->mutations, (unsigned long long)st->flaws,
                   (unsigned long long)st->births, (unsigned long long)st->deaths);
        checksum = checksum * 1000000007ULL
                 + (uint64_t)st->num_cells * 31
                 + (uint64_t)st->num_genotypes * 97
                 + st->births * 13 + st->deaths * 17
                 + st->mutations * 7 + st->flaws * 11
                 + (uint64_t)(st->mem_occupancy * 1e9);
        if (n == 0)
            break; /* slicer queue empty (population extinct) */
    }
    t_destroy(w);
    return checksum;
}

int main(int argc, char **argv) {
    const char *tie_path = argc > 1 ? argv[1] : "assets/0080aaa.tie";
    uint64_t max_inst = 30000000;

    printf("== run 1 (seed=12345) ==\n");
    int32_t mg1, ms1;
    uint64_t c1 = run_once(tie_path, 12345, max_inst, 1, &mg1, &ms1);

    printf("== run 2 (seed=12345, repeat) ==\n");
    int32_t mg2, ms2;
    uint64_t c2 = run_once(tie_path, 12345, max_inst, 0, &mg2, &ms2);

    printf("checksum run1=%llu run2=%llu %s\n",
           (unsigned long long)c1, (unsigned long long)c2,
           c1 == c2 ? "(DETERMINISTIC)" : "(MISMATCH!)");
    printf("max genotypes=%d max sizes=%d %s\n", mg1, ms1,
           (mg1 > 1 && ms1 > 1) ? "(DIVERSITY EMERGED)" : "(NO DIVERSITY)");

    return 0;
}
