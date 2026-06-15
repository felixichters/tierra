#include "genome.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prime modulus from the legacy genio.c Hash(). */
#define GENOME_HASH_PRIME 277218551

int32_t genome_hash(const uint8_t *bytes, int32_t len) {
    int32_t h = 0;
    for (int32_t i = 0; i < len; i++)
        h = (int32_t)((3 * (int64_t)h + bytes[i]) % GENOME_HASH_PRIME);
    return h;
}

void genome_init(TGenebank *gb, int32_t max_size) {
    memset(gb, 0, sizeof(*gb));
    gb->max_size = max_size;
    gb->by_size   = calloc((size_t)max_size + 1, sizeof(*gb->by_size));
    gb->by_size_n = calloc((size_t)max_size + 1, sizeof(*gb->by_size_n));
}

void genome_destroy(TGenebank *gb) {
    for (int32_t i = 0; i < gb->all_n; i++) {
        free(gb->all[i]->genome);
        free(gb->all[i]);
    }
    free(gb->all);
    for (int32_t s = 0; s <= gb->max_size; s++)
        free(gb->by_size[s]);
    free(gb->by_size);
    free(gb->by_size_n);
    memset(gb, 0, sizeof(*gb));
}

/* Linear scan of by_size[size] for a genotype whose genetic-memory region
 * is byte-identical to genome[mg_off..mg_off+mg_size). Mirrors legacy
 * IsInGenQueue: hash is a pre-filter, exact byte comparison decides. */
static int32_t find_match(const TGenebank *gb, int32_t size, int32_t hash,
                           const uint8_t *genome, int32_t mg_off, int32_t mg_size) {
    int32_t n = gb->by_size_n[size];
    for (int32_t i = 0; i < n; i++) {
        int32_t id = gb->by_size[size][i];
        if (id < 0)
            continue;
        const TGenotype *g = gb->all[id];
        if (g->mg_size != mg_size || g->hash != hash)
            continue;
        if (memcmp(g->genome + g->mg_off, genome + mg_off, (size_t)mg_size) == 0)
            return id;
    }
    return -1;
}

int32_t genome_register(TGenebank *gb, const uint8_t *genome, int32_t size,
                         int32_t mg_off, int32_t mg_size,
                         int32_t parent_id, uint64_t origin_inst) {
    int32_t hash = genome_hash(genome + mg_off, mg_size);

    int32_t id = find_match(gb, size, hash, genome, mg_off, mg_size);
    if (id >= 0) {
        gb->all[id]->pop++;
        return id;
    }

    /* Find a free slot in by_size[size] (reuse an extinct genotype's slot),
     * else grow the size class by 4, like legacy NewGenotype. */
    int32_t local_id = -1;
    for (int32_t i = 0; i < gb->by_size_n[size]; i++) {
        if (gb->by_size[size][i] < 0) {
            local_id = i;
            break;
        }
    }
    if (local_id < 0) {
        int32_t old_n = gb->by_size_n[size];
        int32_t new_n = old_n + 4;
        int32_t *grown = realloc(gb->by_size[size], (size_t)new_n * sizeof(int32_t));
        for (int32_t i = old_n; i < new_n; i++)
            grown[i] = -1;
        gb->by_size[size] = grown;
        gb->by_size_n[size] = new_n;
        local_id = old_n;
    }

    TGenotype *g = calloc(1, sizeof(*g));
    g->size = size;
    g->local_id = local_id;
    g->mg_off = mg_off;
    g->mg_size = mg_size;
    g->hash = hash;
    g->parent = parent_id;
    g->pop = 1;
    g->origin_inst = origin_inst;
    g->genome = malloc((size_t)size);
    memcpy(g->genome, genome, (size_t)size);

    if (gb->all_n == gb->all_cap) {
        gb->all_cap = gb->all_cap ? gb->all_cap * 2 : 16;
        gb->all = realloc(gb->all, (size_t)gb->all_cap * sizeof(*gb->all));
    }
    int32_t id_new = gb->all_n++;
    gb->all[id_new] = g;
    gb->by_size[size][local_id] = id_new;
    return id_new;
}

void genome_release(TGenebank *gb, int32_t id) {
    if (id >= 0 && id < gb->all_n && gb->all[id]->pop > 0)
        gb->all[id]->pop--;
}

void genome_name(const TGenebank *gb, int32_t id, char out[8]) {
    if (id < 0 || id >= gb->all_n) {
        out[0] = '\0';
        return;
    }
    const TGenotype *g = gb->all[id];
    int32_t gi = g->local_id;
    snprintf(out, 8, "%04d%c%c%c", g->size % 10000,
             (char)('a' + (gi / (26 * 26)) % 26),
             (char)('a' + (gi / 26) % 26),
             (char)('a' + gi % 26));
}
