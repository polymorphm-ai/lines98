#ifndef FX_PARTICLES_H
#define FX_PARTICLES_H

#include <stdbool.h>

#include <SDL.h>

#include "game.h"

#define MAX_PARTICLES 4096

/* One transient dust particle used by clear-line visual effect. */
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float life;
    SDL_Color color;
    bool active;
} Particle;

/* Fixed-size particle container to avoid heap allocations. */
typedef struct {
    Particle items[MAX_PARTICLES];
} ParticleSystem;

/* Resets all particles to inactive state. */
void particles_init(ParticleSystem *ps);

/* Spawns one particle with randomized velocity and lifetime. */
void particles_spawn_one(ParticleSystem *ps, float x, float y, SDL_Color color);

/* Spawns a burst of particles at one world-space point. */
void particles_spawn_burst(ParticleSystem *ps, float x, float y, SDL_Color color, int count);

/* Advances particle simulation and collisions against board balls. */
void particles_update(
    ParticleSystem *ps,
    float dt,
    const uint8_t *board,
    int board_size,
    int cell_size,
    int board_offset_x,
    int board_offset_y,
    float ball_radius
);

/* Renders all active particles with alpha fade. */
void particles_draw(SDL_Renderer *renderer, const ParticleSystem *ps);

#endif
