#ifndef RNG_H
#define RNG_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} Rng;

void rng_seed(Rng *rng, uint32_t seed);
uint32_t rng_next(Rng *rng);
uint32_t rng_range(Rng *rng, uint32_t upper_exclusive);

#endif
