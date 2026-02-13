/* Small deterministic xorshift RNG used by game logic and tests. */

#include "rng.h"

/* Initializes RNG state and normalizes zero seed. */
void rng_seed(Rng *rng, uint32_t seed) {
    rng->state = seed == 0 ? 0xA341316Cu : seed;
}

/* Produces next pseudo-random 32-bit value. */
uint32_t rng_next(Rng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

/* Produces value in [0, upper_exclusive). */
uint32_t rng_range(Rng *rng, uint32_t upper_exclusive) {
    if (upper_exclusive == 0) {
        return 0;
    }
    return rng_next(rng) % upper_exclusive;
}
