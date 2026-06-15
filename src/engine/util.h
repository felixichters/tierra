#ifndef TIERRA_UTIL_H
#define TIERRA_UTIL_H

#include <stdint.h>

/* ad(size, a) - wrap an address into [0, size). Soup addresses are computed
 * with ordinary signed arithmetic (e.g. ip - 1) and must be folded back into
 * the circular soup; legacy Tierra's ad() macro. */
static inline int32_t tierra_ad(int32_t size, int32_t a) {
    if (a >= 0)
        return a % size;
    return size - ((-a) % size);
}

/* mo(a, b) - floored modulo, used to wrap register indices after a flaw()
 * perturbation; legacy Tierra's mo() macro. */
static inline int32_t tierra_mo(int32_t a, int32_t b) {
    if (a >= 0)
        return a % b;
    return (b - (-a % b)) % b;
}

#endif /* TIERRA_UTIL_H */
