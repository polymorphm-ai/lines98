#include "rng.h"

void rng_seed(Rng *rng, uint32_t seed) {
    rng->state = seed == 0 ? 0xA341316Cu : seed;
}

uint32_t rng_next(Rng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

uint32_t rng_range(Rng *rng, uint32_t upper_exclusive) {
    if (upper_exclusive == 0) {
        return 0;
    }
    return rng_next(rng) % upper_exclusive;
}
