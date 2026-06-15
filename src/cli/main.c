/* tierra-cli: headless runner. Runs the engine for a fixed instruction
 * budget (or until extinction), printing periodic telemetry, then dumps
 * the final genotype table. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tierra/tierra.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [options] [ancestor.tie]\n"
        "\n"
        "  -s, --seed N       PRNG seed (default: 0 = derive from system time)\n"
        "  -n, --max-inst N   stop after N instructions (default: run until extinction)\n"
        "  -r, --report N     print telemetry every N instructions (default: 1000000)\n"
        "      --no-mutation  zero all mutation rates (pure replication run)\n"
        "  -h, --help         show this help\n"
        "\n"
        "ancestor.tie defaults to assets/0080aaa.tie\n",
        prog);
}

static void print_header(void) {
    printf("%12s %8s %6s %5s %5s %10s %7s %12s %10s %10s %10s %10s\n",
           "inst", "gen", "cells", "geno", "sizes", "avg_size", "occ",
           "speed/s", "mut", "flaws", "births", "deaths");
}

static void print_stats(const TStats *st) {
    printf("%12llu %8.2f %6d %5d %5d %10.2f %7.4f %12.0f %10llu %10llu %10llu %10llu\n",
           (unsigned long long)st->inst_executed, st->generations,
           st->num_cells, st->num_genotypes, st->num_sizes,
           st->avg_size, st->mem_occupancy, st->speed,
           (unsigned long long)st->mutations, (unsigned long long)st->flaws,
           (unsigned long long)st->births, (unsigned long long)st->deaths);
}

int main(int argc, char **argv) {
    const char *tie_path = "assets/0080aaa.tie";
    uint64_t seed = 0;
    uint64_t max_inst = 0;
    uint64_t report = 1000000;
    int no_mutation = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((!strcmp(a, "-s") || !strcmp(a, "--seed")) && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if ((!strcmp(a, "-n") || !strcmp(a, "--max-inst")) && i + 1 < argc) {
            max_inst = strtoull(argv[++i], NULL, 10);
        } else if ((!strcmp(a, "-r") || !strcmp(a, "--report")) && i + 1 < argc) {
            report = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(a, "--no-mutation")) {
            no_mutation = 1;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(argv[0]);
            return 0;
        } else if (a[0] == '-') {
            fprintf(stderr, "%s: unknown option '%s'\n", argv[0], a);
            usage(argv[0]);
            return 1;
        } else {
            tie_path = a;
        }
    }

    if (report == 0) {
        fprintf(stderr, "%s: --report must be > 0\n", argv[0]);
        return 1;
    }

    TConfig cfg = t_config_default();
    cfg.seed = seed;
    if (no_mutation) {
        cfg.gen_per_bkg_mut = 0;
        cfg.gen_per_flaw = 0;
        cfg.gen_per_mov_mut = 0;
        cfg.gen_per_div_mut = 0;
        cfg.gen_per_ins_ins = 0;
        cfg.gen_per_del_ins = 0;
        cfg.gen_per_cro_ins = 0;
        cfg.gen_per_ins_seg = 0;
        cfg.gen_per_del_seg = 0;
        cfg.gen_per_cro_seg = 0;
        cfg.gen_per_cro_ins_sam_siz = 0;
    }

    TWorld *w = t_create(&cfg);
    if (!w) {
        fprintf(stderr, "%s: t_create failed\n", argv[0]);
        return 1;
    }

    if (t_seed_file(w, tie_path) != 0) {
        fprintf(stderr, "%s: failed to load '%s'\n", argv[0], tie_path);
        t_destroy(w);
        return 1;
    }

    printf("tierra-cli: '%s' soup_size=%d seed=%llu%s\n",
           tie_path, cfg.soup_size, (unsigned long long)cfg.seed,
           no_mutation ? " (mutation disabled)" : "");
    print_header();

    uint64_t total = 0;
    for (;;) {
        uint64_t chunk = report;
        if (max_inst > 0) {
            uint64_t remaining = max_inst - total;
            if (remaining < chunk)
                chunk = remaining;
            if (chunk == 0)
                break;
        }

        uint64_t n = t_step(w, chunk);
        total += n;

        print_stats(t_stats(w));

        if (n == 0) {
            printf("population extinct at inst=%llu\n", (unsigned long long)total);
            break;
        }
        if (max_inst > 0 && total >= max_inst)
            break;
    }

    int ngeno = 0;
    const TGenoView *gv = t_genotypes(w, &ngeno);
    printf("\n%-8s %8s %8s %8s\n", "name", "size", "pop", "id");
    for (int i = 0; i < ngeno; i++)
        printf("%-8s %8d %8d %8d\n", gv[i].name, gv[i].size, gv[i].pop, gv[i].id);

    t_destroy(w);
    return 0;
}
