/* Particle simulation and rendering for line-clear dust effect.
   This module is pure fixed-size state and does not allocate memory. */

#include "fx_particles.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Clamps color alpha/brightness mixing towards white. */
static SDL_Color mix_white(SDL_Color c, float t) {
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    SDL_Color out = c;
    out.r = (uint8_t)(c.r + (255 - c.r) * t);
    out.g = (uint8_t)(c.g + (255 - c.g) * t);
    out.b = (uint8_t)(c.b + (255 - c.b) * t);
    return out;
}

/* Draws a solid filled circle using scanlines. */
static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -radius; y <= radius; ++y) {
        int span = (int)sqrt((double)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
}

/* Computes ball center X for board cell coordinates. */
static int ball_center_x(int col, int cell_size, int board_offset_x) {
    return board_offset_x + col * cell_size + cell_size / 2;
}

/* Computes ball center Y for board cell coordinates. */
static int ball_center_y(int row, int cell_size, int board_offset_y) {
    return board_offset_y + row * cell_size + cell_size / 2;
}

/* Resets all particles to inactive state. */
void particles_init(ParticleSystem *ps) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        ps->items[i].active = false;
    }
}

/* Spawns one particle with randomized velocity and lifetime. */
void particles_spawn_one(ParticleSystem *ps, float x, float y, SDL_Color color) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!ps->items[i].active) {
            float a = (float)rand() / (float)RAND_MAX * 2.0f * (float)M_PI;
            float s = 70.0f + ((float)rand() / (float)RAND_MAX) * 240.0f;
            float jx = (((float)rand() / (float)RAND_MAX) - 0.5f) * 10.0f;
            float jy = (((float)rand() / (float)RAND_MAX) - 0.5f) * 10.0f;

            ps->items[i].active = true;
            ps->items[i].x = x + jx;
            ps->items[i].y = y + jy;
            ps->items[i].vx = cosf(a) * s;
            ps->items[i].vy = sinf(a) * s - 30.0f;
            ps->items[i].radius = 1.8f + ((float)rand() / (float)RAND_MAX) * 2.2f;
            ps->items[i].life = 0.7f + ((float)rand() / (float)RAND_MAX) * 0.9f;
            ps->items[i].color = mix_white(color, 0.25f);
            return;
        }
    }
}

/* Spawns a burst of particles at one world-space point. */
void particles_spawn_burst(ParticleSystem *ps, float x, float y, SDL_Color color, int count) {
    for (int i = 0; i < count; ++i) {
        particles_spawn_one(ps, x, y, color);
    }
}

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
) {
    const float gravity = 560.0f;
    const float bounce = 0.58f;
    const int board_w = board_size * cell_size;
    const int board_h = board_size * cell_size;
    const float min_x = (float)board_offset_x;
    const float max_x = (float)(board_offset_x + board_w);
    const float min_y = (float)board_offset_y;
    const float max_y = (float)(board_offset_y + board_h);

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &ps->items[i];
        if (!p->active) {
            continue;
        }

        p->life -= dt;
        if (p->life <= 0.0f) {
            p->active = false;
            continue;
        }

        p->vy += gravity * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        if (p->x < min_x + p->radius) {
            p->x = min_x + p->radius;
            p->vx = -p->vx * bounce;
        } else if (p->x > max_x - p->radius) {
            p->x = max_x - p->radius;
            p->vx = -p->vx * bounce;
        }

        if (p->y < min_y + p->radius) {
            p->y = min_y + p->radius;
            p->vy = -p->vy * bounce;
        } else if (p->y > max_y - p->radius) {
            p->y = max_y - p->radius;
            p->vy = -p->vy * bounce;
            p->vx *= 0.88f;
        }

        for (int row = 0; row < board_size; ++row) {
            for (int col = 0; col < board_size; ++col) {
                uint8_t cell = board[row * board_size + col];
                if (cell == 0) {
                    continue;
                }

                float ox = (float)ball_center_x(col, cell_size, board_offset_x);
                float oy = (float)ball_center_y(row, cell_size, board_offset_y);
                float dx = p->x - ox;
                float dy = p->y - oy;
                float min_dist = p->radius + ball_radius;
                float d2 = dx * dx + dy * dy;
                if (d2 >= min_dist * min_dist) {
                    continue;
                }

                float d = sqrtf(SDL_max(d2, 0.0001f));
                float nx = dx / d;
                float ny = dy / d;
                p->x = ox + nx * min_dist;
                p->y = oy + ny * min_dist;

                float vn = p->vx * nx + p->vy * ny;
                if (vn < 0.0f) {
                    p->vx -= (1.0f + bounce) * vn * nx;
                    p->vy -= (1.0f + bounce) * vn * ny;
                    p->vx *= 0.94f;
                    p->vy *= 0.94f;
                }
            }
        }
    }
}

/* Renders all active particles with alpha fade. */
void particles_draw(SDL_Renderer *renderer, const ParticleSystem *ps) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle *p = &ps->items[i];
        if (!p->active) {
            continue;
        }
        SDL_Color c = p->color;
        c.a = (uint8_t)SDL_min(255, (int)(255.0f * SDL_min(1.0f, p->life)));
        draw_filled_circle(renderer, (int)p->x, (int)p->y, (int)p->radius, c);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
