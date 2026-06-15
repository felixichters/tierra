/* Throwaway smoke test for step 3 (genebank/slicer/reaper/world loop).
 * Verifies: population grows from 1 and fills the soup, exactly one
 * genotype is present (mutation operators are step 4, still disabled),
 * and the run is deterministic for a fixed seed.
 *
 * Build: clang -std=c11 -Iinclude tests/smoke3.c -L. -ltierra -lm -o /tmp/smoke3
 * Run:   /tmp/smoke3 assets/0080aaa.tie
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tierra/tierra.h"

static uint64_t run_once(const char *tie_path, uint64_t seed, uint64_t max_inst,
                          int verbose) {
    TConfig cfg = t_config_default();
    cfg.seed = seed;

    TWorld *w = t_create(&cfg);
    if (t_seed_file(w, tie_path) != 0) {
        fprintf(stderr, "t_seed_file failed for %s\n", tie_path);
        exit(1);
    }

    uint64_t done = 0;
    uint64_t chunk = 100000;
    uint64_t checksum = 0;
    while (done < max_inst) {
        uint64_t n = t_step(w, chunk);
        done += n;
        const TStats *st = t_stats(w);
        if (verbose)
            printf("  inst=%-9llu cells=%-5d genotypes=%-3d occ=%.4f births=%llu deaths=%llu\n",
                   (unsigned long long)st->inst_executed, st->num_cells,
                   st->num_genotypes, st->mem_occupancy,
                   (unsigned long long)st->births, (unsigned long long)st->deaths);
        checksum = checksum * 1000000007ULL
                 + (uint64_t)st->num_cells * 31
                 + (uint64_t)st->num_genotypes * 97
                 + st->births * 13 + st->deaths * 17
                 + (uint64_t)(st->mem_occupancy * 1e9);
        if (n == 0)
            break; /* slicer queue empty (population extinct) */
    }
    t_destroy(w);
    return checksum;
}

int main(int argc, char **argv) {
    const char *tie_path = argc > 1 ? argv[1] : "assets/0080aaa.tie";
    uint64_t max_inst = 2000000;

    printf("== run 1 (seed=12345) ==\n");
    uint64_t c1 = run_once(tie_path, 12345, max_inst, 1);

    printf("== run 2 (seed=12345, repeat) ==\n");
    uint64_t c2 = run_once(tie_path, 12345, max_inst, 0);

    printf("checksum run1=%llu run2=%llu %s\n",
           (unsigned long long)c1, (unsigned long long)c2,
           c1 == c2 ? "(DETERMINISTIC)" : "(MISMATCH!)");

    return 0;
}
