#include "rng.h"

/* splitmix64, used only to spread a single seed across xoshiro's 256-bit state. */
static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void rng_seed(TRng *rng, uint64_t seed) {
    uint64_t sm = seed;
    for (int i = 0; i < 4; i++) {
        rng->s[i] = splitmix64_next(&sm);
    }
}

static inline uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t rng_u64(TRng *rng) {
    uint64_t *s = rng->s;
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);

    return result;
}

uint32_t rng_below(TRng *rng, uint32_t bound) {
    if (bound == 0) return 0;
    return (uint32_t)(rng_u64(rng) % bound);
}

double rng_double(TRng *rng) {
    return (double)(rng_u64(rng) >> 11) * (1.0 / 9007199254740992.0 /* 2^53 */);
}
