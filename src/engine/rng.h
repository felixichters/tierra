/* xoshiro256** PRNG, per-world state for reproducible runs. */
#ifndef TIERRA_RNG_H
#define TIERRA_RNG_H

#include <stdint.h>

typedef struct TRng {
    uint64_t s[4];
} TRng;

void     rng_seed(TRng *rng, uint64_t seed);
uint64_t rng_u64(TRng *rng);

/* Uniform integer in [0, bound). */
uint32_t rng_below(TRng *rng, uint32_t bound);

/* Uniform double in [0, 1). */
double   rng_double(TRng *rng);

#endif /* TIERRA_RNG_H */
