#ifndef TIERRA_GENOME_H
#define TIERRA_GENOME_H

#include <stdint.h>

/* One genotype: a distinct genome, identified by the byte content of its
 * "genetic memory" region (the span actually copied by movii during the
 * reproduction that created it). Mirrors the essential fields of legacy
 * GList, dropping disk/XDR/thread-analysis bookkeeping. */
typedef struct TGenotype {
    int32_t  size;      /* mm.size: full genome length, in instructions */
    int32_t  local_id;  /* index within its size class -> base-26 label */
    int32_t  mg_off;    /* genetic-memory region: offset within genome[] */
    int32_t  mg_size;   /* ... and length; this is what defines identity */
    int32_t  hash;
    int32_t  parent;    /* global id of the parent genotype, -1 if none */
    int32_t  pop;       /* current population of this genotype */
    uint64_t origin_inst;
    uint8_t *genome;    /* copy of the full genome, `size` bytes */
} TGenotype;

/* The in-RAM genebank: genotypes grouped by genome size, like legacy
 * sl[size]->g[]. No disk genebank, no XDR. */
typedef struct TGenebank {
    TGenotype **all;
    int32_t      all_n, all_cap;

    /* by_size[size] is a dynamic array of indices into `all` (or -1 for an
     * unused slot), one array per genome size in [0, max_size]. */
    int32_t **by_size;
    int32_t  *by_size_n;
    int32_t   max_size;
} TGenebank;

void genome_init(TGenebank *gb, int32_t max_size);
void genome_destroy(TGenebank *gb);

/* Polynomial hash of `len` bytes, per the legacy genio.c Hash():
 * h = (3*h + byte) % 277218551. */
int32_t genome_hash(const uint8_t *bytes, int32_t len);

/* Find or mint the genotype for a `size`-byte genome whose genetic-memory
 * region is genome[mg_off..mg_off+mg_size). On a byte-exact match within
 * the same size class, increments that genotype's population and returns
 * its id; otherwise mints a new genotype (pop=1, parent=parent_id, a copy
 * of `genome` is taken) and returns its id. */
int32_t genome_register(TGenebank *gb, const uint8_t *genome, int32_t size,
                         int32_t mg_off, int32_t mg_size,
                         int32_t parent_id, uint64_t origin_inst);

/* Decrement a genotype's population (called when a cell dies). */
void genome_release(TGenebank *gb, int32_t id);

/* "NNNNlll" style name, e.g. "0080aaa": 4-digit genome size (mod 10000)
 * followed by the base-26 label for `local_id` (aaa, aab, ... zzz).
 * `out` must hold at least 8 bytes. */
void genome_name(const TGenebank *gb, int32_t id, char out[8]);

#endif /* TIERRA_GENOME_H */
