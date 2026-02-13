#include <SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 840
#define BOARD_OFFSET_X 92
#define BOARD_OFFSET_Y 110
#define CELL_SIZE 64
#define NEXT_OFFSET_X 92
#define NEXT_OFFSET_Y 36
#define MAX_PARTICLES 4096
#define MAX_PATH_NODES GAME_CELLS

typedef struct {
    bool active;
    int path[MAX_PATH_NODES];
    int path_len;
    uint8_t color;
} MoveAnim;

typedef enum {
    TURN_PHASE_NONE = 0,
    TURN_PHASE_MOVE = 1,
    TURN_PHASE_CLEAR = 2,
    TURN_PHASE_SPAWN = 3
} TurnPhase;

typedef struct {
    bool active;
    TurnPhase phase;
    float phase_t;
    float move_dur;
    float clear_dur;
    float spawn_dur;
    MoveAnim move;
    int cleared_idx[GAME_CELLS];
    uint8_t cleared_color[GAME_CELLS];
    int cleared_count;
    int spawned_idx[GAME_CELLS];
    uint8_t spawned_color[GAME_CELLS];
    int spawned_count;
    uint8_t before_board[GAME_CELLS];
    uint8_t after_move_board[GAME_CELLS];
    uint8_t after_clear_board[GAME_CELLS];
    uint8_t final_board[GAME_CELLS];
} TurnAnim;

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

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    bool audio_ready;
    Game game;
    uint8_t render_board[GAME_CELLS];
    TurnAnim turn_anim;
    Particle particles[MAX_PARTICLES];
} App;

static const SDL_Color BG = {22, 26, 34, 255};
static const SDL_Color GRID_BG = {33, 39, 49, 255};
static const SDL_Color GRID_LINE = {64, 76, 92, 255};
static const SDL_Color SELECTED = {245, 245, 245, 255};
static const SDL_Color TEXT = {218, 230, 247, 255};

static const SDL_Color BALL_COLORS[GAME_COLORS + 1] = {
    {0, 0, 0, 255},
    {229, 73, 81, 255},
    {255, 156, 71, 255},
    {248, 225, 68, 255},
    {110, 207, 93, 255},
    {83, 186, 230, 255},
    {80, 118, 228, 255},
    {181, 106, 214, 255}
};

static void set_color(SDL_Renderer *renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static SDL_Color scale_color(SDL_Color c, float k) {
    if (k < 0.0f) {
        k = 0.0f;
    }
    SDL_Color out = c;
    out.r = (uint8_t)SDL_min(255, (int)(c.r * k));
    out.g = (uint8_t)SDL_min(255, (int)(c.g * k));
    out.b = (uint8_t)SDL_min(255, (int)(c.b * k));
    return out;
}

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

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    set_color(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        int span = (int)sqrt((double)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
}

static void draw_ball(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color base) {
    draw_filled_circle(renderer, cx + 2, cy + 3, radius, scale_color(base, 0.28f));
    draw_filled_circle(renderer, cx, cy, radius, scale_color(base, 0.72f));
    draw_filled_circle(renderer, cx - 1, cy - 1, (radius * 8) / 10, base);
    draw_filled_circle(renderer, cx - 5, cy - 6, radius / 2, mix_white(base, 0.65f));
    draw_filled_circle(renderer, cx - 8, cy - 9, SDL_max(2, radius / 6), mix_white(base, 0.9f));
}

static int ball_center_x(int col) {
    return BOARD_OFFSET_X + col * CELL_SIZE + CELL_SIZE / 2;
}

static int ball_center_y(int row) {
    return BOARD_OFFSET_Y + row * CELL_SIZE + CELL_SIZE / 2;
}

static int rc_to_idx(int row, int col) {
    return row * GAME_BOARD_SIZE + col;
}

static bool idx_to_rc(int idx, int *row, int *col) {
    if (idx < 0 || idx >= GAME_CELLS) {
        return false;
    }
    *row = idx / GAME_BOARD_SIZE;
    *col = idx % GAME_BOARD_SIZE;
    return true;
}

static bool in_bounds(int row, int col) {
    return row >= 0 && row < GAME_BOARD_SIZE && col >= 0 && col < GAME_BOARD_SIZE;
}

static void sync_render_board(App *app) {
    memcpy(app->render_board, app->game.board, sizeof(app->render_board));
}

static bool build_move_path(const uint8_t *board, int from_idx, int to_idx, int *path, int *path_len) {
    if (from_idx < 0 || from_idx >= GAME_CELLS || to_idx < 0 || to_idx >= GAME_CELLS) {
        return false;
    }
    if (from_idx == to_idx) {
        path[0] = from_idx;
        *path_len = 1;
        return true;
    }

    int prev[GAME_CELLS];
    for (int i = 0; i < GAME_CELLS; ++i) {
        prev[i] = -1;
    }

    int queue[GAME_CELLS];
    int head = 0;
    int tail = 0;
    queue[tail++] = from_idx;
    prev[from_idx] = from_idx;

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    bool found = false;

    while (head < tail && !found) {
        int cur = queue[head++];
        int row;
        int col;
        (void)idx_to_rc(cur, &row, &col);
        for (int k = 0; k < 4; ++k) {
            int nr = row + dr[k];
            int nc = col + dc[k];
            if (!in_bounds(nr, nc)) {
                continue;
            }
            int nxt = rc_to_idx(nr, nc);
            if (prev[nxt] != -1) {
                continue;
            }
            if (nxt != to_idx && board[nxt] != 0) {
                continue;
            }

            prev[nxt] = cur;
            if (nxt == to_idx) {
                found = true;
                break;
            }
            queue[tail++] = nxt;
        }
    }

    if (!found) {
        return false;
    }

    int rev[MAX_PATH_NODES];
    int n = 0;
    for (int cur = to_idx; cur != from_idx; cur = prev[cur]) {
        rev[n++] = cur;
    }
    rev[n++] = from_idx;

    for (int i = 0; i < n; ++i) {
        path[i] = rev[n - 1 - i];
    }
    *path_len = n;
    return true;
}

static void clear_turn_anim(App *app) {
    memset(&app->turn_anim, 0, sizeof(app->turn_anim));
}

static void clear_particles(App *app) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        app->particles[i].active = false;
    }
}

static void spawn_particle(App *app, float x, float y, SDL_Color color) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!app->particles[i].active) {
            float a = (float)rand() / (float)RAND_MAX * 2.0f * (float)M_PI;
            float s = 70.0f + ((float)rand() / (float)RAND_MAX) * 240.0f;
            float jx = (((float)rand() / (float)RAND_MAX) - 0.5f) * 10.0f;
            float jy = (((float)rand() / (float)RAND_MAX) - 0.5f) * 10.0f;

            app->particles[i].active = true;
            app->particles[i].x = x + jx;
            app->particles[i].y = y + jy;
            app->particles[i].vx = cosf(a) * s;
            app->particles[i].vy = sinf(a) * s - 30.0f;
            app->particles[i].radius = 1.8f + ((float)rand() / (float)RAND_MAX) * 2.2f;
            app->particles[i].life = 0.7f + ((float)rand() / (float)RAND_MAX) * 0.9f;
            app->particles[i].color = mix_white(color, 0.25f);
            return;
        }
    }
}

static void emit_clear_particles(App *app, const TurnAnim *anim) {
    for (int i = 0; i < anim->cleared_count; ++i) {
        int idx = anim->cleared_idx[i];
        int row;
        int col;
        if (!idx_to_rc(idx, &row, &col)) {
            continue;
        }

        float x = (float)ball_center_x(col);
        float y = (float)ball_center_y(row);
        SDL_Color color = BALL_COLORS[anim->cleared_color[i]];
        for (int k = 0; k < 18; ++k) {
            spawn_particle(app, x, y, color);
        }
    }
}

static void start_turn_animation(
    App *app,
    const uint8_t *before,
    int from_idx,
    int to_idx,
    const int *path,
    int path_len
) {
    TurnAnim anim;
    memset(&anim, 0, sizeof(anim));

    anim.active = true;
    anim.phase = TURN_PHASE_MOVE;
    anim.phase_t = 0.0f;
    anim.move_dur = 0.18f;
    anim.clear_dur = 0.16f;
    anim.spawn_dur = 0.18f;

    anim.move.active = path_len >= 2;
    anim.move.path_len = path_len;
    anim.move.color = (from_idx >= 0 && from_idx < GAME_CELLS) ? before[from_idx] : 0;
    if (anim.move.active) {
        memcpy(anim.move.path, path, (size_t)path_len * sizeof(int));
    }

    memcpy(anim.before_board, before, sizeof(anim.before_board));
    memcpy(anim.final_board, app->game.board, sizeof(anim.final_board));
    memcpy(anim.after_move_board, before, sizeof(anim.after_move_board));
    if (from_idx >= 0 && from_idx < GAME_CELLS && to_idx >= 0 && to_idx < GAME_CELLS) {
        anim.after_move_board[to_idx] = anim.after_move_board[from_idx];
        anim.after_move_board[from_idx] = 0;
    }
    memcpy(anim.after_clear_board, anim.after_move_board, sizeof(anim.after_clear_board));

    bool moved_survived = false;
    if (to_idx >= 0 && to_idx < GAME_CELLS && anim.move.color != 0) {
        moved_survived = app->game.board[to_idx] == anim.move.color;
    }

    for (int idx = 0; idx < GAME_CELLS; ++idx) {
        if (before[idx] != 0 && app->game.board[idx] == 0 && idx != from_idx) {
            anim.cleared_idx[anim.cleared_count] = idx;
            anim.cleared_color[anim.cleared_count] = before[idx];
            ++anim.cleared_count;
            anim.after_clear_board[idx] = 0;
        } else if (before[idx] == 0 && app->game.board[idx] != 0) {
            if (idx == to_idx && moved_survived) {
                continue;
            }
            anim.spawned_idx[anim.spawned_count] = idx;
            anim.spawned_color[anim.spawned_count] = app->game.board[idx];
            ++anim.spawned_count;
        }
    }

    if (to_idx >= 0 && to_idx < GAME_CELLS && anim.move.color != 0 && !moved_survived) {
        bool already_added = false;
        for (int i = 0; i < anim.cleared_count; ++i) {
            if (anim.cleared_idx[i] == to_idx) {
                already_added = true;
                break;
            }
        }
        if (!already_added) {
            anim.cleared_idx[anim.cleared_count] = to_idx;
            anim.cleared_color[anim.cleared_count] = anim.move.color;
            ++anim.cleared_count;
        }
    }

    if (from_idx >= 0 && from_idx < GAME_CELLS) {
        app->render_board[from_idx] = 0;
    }
    if (to_idx >= 0 && to_idx < GAME_CELLS && !moved_survived) {
        app->render_board[to_idx] = 0;
    }

    memcpy(app->render_board, anim.before_board, sizeof(app->render_board));
    if (from_idx >= 0 && from_idx < GAME_CELLS) {
        app->render_board[from_idx] = 0;
    }

    app->turn_anim = anim;
}

static float spawned_scale_for_index(const TurnAnim *anim, int idx) {
    if (!anim->active || anim->phase != TURN_PHASE_SPAWN) {
        return -1.0f;
    }

    int found = -1;
    for (int i = 0; i < anim->spawned_count; ++i) {
        if (anim->spawned_idx[i] == idx) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return -1.0f;
    }

    float k = anim->phase_t / anim->spawn_dur;
    if (k > 1.0f) {
        k = 1.0f;
    }
    if (k < 0.0f) {
        k = 0.0f;
    }
    return k;
}

static void update_turn_animation(App *app, float dt) {
    TurnAnim *anim = &app->turn_anim;
    if (!anim->active) {
        return;
    }

    anim->phase_t += dt;

    if (anim->phase == TURN_PHASE_MOVE) {
        if (anim->phase_t >= anim->move_dur) {
            anim->phase = TURN_PHASE_CLEAR;
            anim->phase_t = 0.0f;
            memcpy(app->render_board, anim->after_move_board, sizeof(app->render_board));
            emit_clear_particles(app, anim);
            memcpy(app->render_board, anim->after_clear_board, sizeof(app->render_board));
            if (anim->cleared_count == 0) {
                anim->phase = TURN_PHASE_SPAWN;
                anim->phase_t = 0.0f;
                memcpy(app->render_board, anim->after_clear_board, sizeof(app->render_board));
            }
        }
        return;
    }

    if (anim->phase == TURN_PHASE_CLEAR) {
        if (anim->phase_t >= anim->clear_dur) {
            anim->phase = TURN_PHASE_SPAWN;
            anim->phase_t = 0.0f;
            memcpy(app->render_board, anim->after_clear_board, sizeof(app->render_board));
        }
        return;
    }

    if (anim->phase == TURN_PHASE_SPAWN) {
        memcpy(app->render_board, anim->after_clear_board, sizeof(app->render_board));
        for (int i = 0; i < anim->spawned_count; ++i) {
            int idx = anim->spawned_idx[i];
            app->render_board[idx] = anim->spawned_color[i];
        }
        if (anim->phase_t >= anim->spawn_dur) {
            memcpy(app->render_board, anim->final_board, sizeof(app->render_board));
            clear_turn_anim(app);
        }
        return;
    }
}

static void update_particles(App *app, float dt) {
    const float gravity = 560.0f;
    const float bounce = 0.58f;
    const int board_w = GAME_BOARD_SIZE * CELL_SIZE;
    const int board_h = GAME_BOARD_SIZE * CELL_SIZE;
    const float min_x = (float)BOARD_OFFSET_X;
    const float max_x = (float)(BOARD_OFFSET_X + board_w);
    const float min_y = (float)BOARD_OFFSET_Y;
    const float max_y = (float)(BOARD_OFFSET_Y + board_h);

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &app->particles[i];
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

        for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
            for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
                uint8_t cell = app->render_board[rc_to_idx(row, col)];
                if (cell == 0) {
                    continue;
                }

                float ox = (float)ball_center_x(col);
                float oy = (float)ball_center_y(row);
                float dx = p->x - ox;
                float dy = p->y - oy;
                float min_dist = p->radius + 23.0f;
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

static void draw_particles(SDL_Renderer *renderer, const App *app) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle *p = &app->particles[i];
        if (!p->active) {
            continue;
        }
        SDL_Color c = p->color;
        c.a = (uint8_t)SDL_min(255, (int)(255.0f * SDL_min(1.0f, p->life)));
        draw_filled_circle(renderer, (int)p->x, (int)p->y, (int)p->radius, c);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_move_animation(SDL_Renderer *renderer, const App *app) {
    const TurnAnim *anim = &app->turn_anim;
    if (!anim->active || anim->phase != TURN_PHASE_MOVE || !anim->move.active || anim->move.color == 0 || anim->move.path_len < 2) {
        return;
    }

    float u = (anim->phase_t / anim->move_dur) * (float)(anim->move.path_len - 1);
    if (u < 0.0f) {
        u = 0.0f;
    }
    if (u > (float)(anim->move.path_len - 1)) {
        u = (float)(anim->move.path_len - 1);
    }

    int seg = (int)u;
    if (seg >= anim->move.path_len - 1) {
        seg = anim->move.path_len - 2;
        u = (float)(anim->move.path_len - 1);
    }

    float frac = u - (float)seg;
    int idx0 = anim->move.path[seg];
    int idx1 = anim->move.path[seg + 1];
    int r0;
    int c0;
    int r1;
    int c1;
    if (!idx_to_rc(idx0, &r0, &c0) || !idx_to_rc(idx1, &r1, &c1)) {
        return;
    }

    float x0 = (float)ball_center_x(c0);
    float y0 = (float)ball_center_y(r0);
    float x1 = (float)ball_center_x(c1);
    float y1 = (float)ball_center_y(r1);
    int cx = (int)(x0 + (x1 - x0) * frac);
    int cy = (int)(y0 + (y1 - y0) * frac);
    draw_ball(renderer, cx, cy, 23, BALL_COLORS[anim->move.color]);
}

static void draw_segment(SDL_Renderer *renderer, int x, int y, int w, int h, bool on) {
    if (!on) {
        return;
    }
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

static void draw_digit(SDL_Renderer *renderer, int x, int y, int scale, int digit) {
    static const bool segments[10][7] = {
        {true, true, true, true, true, true, false},
        {false, true, true, false, false, false, false},
        {true, true, false, true, true, false, true},
        {true, true, true, true, false, false, true},
        {false, true, true, false, false, true, true},
        {true, false, true, true, false, true, true},
        {true, false, true, true, true, true, true},
        {true, true, true, false, false, false, false},
        {true, true, true, true, true, true, true},
        {true, true, true, true, false, true, true}
    };

    int t = scale;
    int lw = 6 * scale;
    int lh = 10 * scale;

    draw_segment(renderer, x + t, y, lw, t, segments[digit][0]);
    draw_segment(renderer, x + lw + t, y + t, t, lh, segments[digit][1]);
    draw_segment(renderer, x + lw + t, y + lh + 2 * t, t, lh, segments[digit][2]);
    draw_segment(renderer, x + t, y + 2 * lh + 2 * t, lw, t, segments[digit][3]);
    draw_segment(renderer, x, y + lh + 2 * t, t, lh, segments[digit][4]);
    draw_segment(renderer, x, y + t, t, lh, segments[digit][5]);
    draw_segment(renderer, x + t, y + lh + t, lw, t, segments[digit][6]);
}

static void draw_glyph(SDL_Renderer *renderer, int x, int y, int scale, char ch) {
    static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 14};
    static const uint8_t glyph_m[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};

    const uint8_t *glyph = glyph_space;
    switch (ch) {
        case 'A':
            glyph = glyph_a;
            break;
        case 'C':
            glyph = glyph_c;
            break;
        case 'E':
            glyph = glyph_e;
            break;
        case 'G':
            glyph = glyph_g;
            break;
        case 'M':
            glyph = glyph_m;
            break;
        case 'O':
            glyph = glyph_o;
            break;
        case 'R':
            glyph = glyph_r;
            break;
        case 'S':
            glyph = glyph_s;
            break;
        case 'V':
            glyph = glyph_v;
            break;
        default:
            glyph = glyph_space;
            break;
    }

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((glyph[row] >> (4 - col)) & 1u) {
                SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &px);
            }
        }
    }
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, const char *text) {
    int cursor = x;
    for (const char *p = text; *p != '\0'; ++p) {
        draw_glyph(renderer, cursor, y, scale, *p);
        cursor += 6 * scale;
    }
}

static int text_pixel_width(int scale, const char *text) {
    int len = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        ++len;
    }
    return len * 6 * scale;
}

static void draw_score(SDL_Renderer *renderer, int score) {
    if (score < 0) {
        score = 0;
    }
    if (score > 9999) {
        score = 9999;
    }

    set_color(renderer, TEXT);
    int d0 = (score / 1000) % 10;
    int d1 = (score / 100) % 10;
    int d2 = (score / 10) % 10;
    int d3 = score % 10;

    draw_digit(renderer, 520, 18, 2, d0);
    draw_digit(renderer, 560, 18, 2, d1);
    draw_digit(renderer, 600, 18, 2, d2);
    draw_digit(renderer, 640, 18, 2, d3);
}

static void play_tone(App *app, float frequency, int duration_ms, float gain) {
    if (!app->audio_ready || duration_ms <= 0) {
        return;
    }

    int sample_count = (app->audio_spec.freq * duration_ms) / 1000;
    if (sample_count <= 0) {
        return;
    }

    float *samples = (float *)malloc((size_t)sample_count * sizeof(float));
    if (samples == NULL) {
        return;
    }

    float phase = 0.0f;
    float step = 2.0f * (float)M_PI * frequency / (float)app->audio_spec.freq;
    for (int i = 0; i < sample_count; ++i) {
        float env = 1.0f - (float)i / (float)sample_count;
        samples[i] = sinf(phase) * gain * env;
        phase += step;
    }

    SDL_QueueAudio(app->audio_device, samples, (Uint32)(sample_count * (int)sizeof(float)));
    free(samples);
}

static bool app_init(App *app) {
    memset(app, 0, sizeof(*app));
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    app->window = SDL_CreateWindow(
        "Lines-98 in C/SDL2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (app->window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (app->renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;

    app->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &app->audio_spec, 0);
    if (app->audio_device != 0) {
        SDL_PauseAudioDevice(app->audio_device, 0);
        app->audio_ready = true;
    }

    game_init(&app->game, (uint32_t)time(NULL));
    sync_render_board(app);
    clear_turn_anim(app);
    clear_particles(app);
    return true;
}

static void app_shutdown(App *app) {
    if (app->audio_device != 0) {
        SDL_ClearQueuedAudio(app->audio_device);
        SDL_CloseAudioDevice(app->audio_device);
        app->audio_device = 0;
    }

    if (app->renderer != NULL) {
        SDL_DestroyRenderer(app->renderer);
        app->renderer = NULL;
    }

    if (app->window != NULL) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

static void draw_next_balls(SDL_Renderer *renderer, const Game *game) {
    for (int i = 0; i < GAME_NEXT_COUNT; ++i) {
        int x = NEXT_OFFSET_X + i * 56;
        int y = NEXT_OFFSET_Y;
        draw_ball(renderer, x, y, 18, BALL_COLORS[game->next_colors[i]]);
    }
}

static void draw_board(SDL_Renderer *renderer, const App *app) {
    SDL_Rect board_rect = {BOARD_OFFSET_X, BOARD_OFFSET_Y, GAME_BOARD_SIZE * CELL_SIZE, GAME_BOARD_SIZE * CELL_SIZE};
    set_color(renderer, GRID_BG);
    SDL_RenderFillRect(renderer, &board_rect);

    set_color(renderer, GRID_LINE);
    for (int i = 0; i <= GAME_BOARD_SIZE; ++i) {
        int x = BOARD_OFFSET_X + i * CELL_SIZE;
        int y = BOARD_OFFSET_Y + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, x, BOARD_OFFSET_Y, x, BOARD_OFFSET_Y + GAME_BOARD_SIZE * CELL_SIZE);
        SDL_RenderDrawLine(renderer, BOARD_OFFSET_X, y, BOARD_OFFSET_X + GAME_BOARD_SIZE * CELL_SIZE, y);
    }

    int sel_row = -1;
    int sel_col = -1;
    if (!app->turn_anim.active && app->game.selected_index >= 0) {
        sel_row = app->game.selected_index / GAME_BOARD_SIZE;
        sel_col = app->game.selected_index % GAME_BOARD_SIZE;
    }

    for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
        for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
            int idx = rc_to_idx(row, col);
            uint8_t cell = app->turn_anim.active ? app->render_board[idx] : app->game.board[idx];
            if (cell == 0) {
                continue;
            }

            int cx = ball_center_x(col);
            int cy = ball_center_y(row);
            int radius = 23;
            if (app->turn_anim.active) {
                float s = spawned_scale_for_index(&app->turn_anim, idx);
                if (s >= 0.0f) {
                    radius = (int)(2.0f + 21.0f * s);
                }
            }
            draw_ball(renderer, cx, cy, radius, BALL_COLORS[cell]);

            if (row == sel_row && col == sel_col) {
                set_color(renderer, SELECTED);
                SDL_Rect marker = {cx - 26, cy - 26, 52, 52};
                SDL_RenderDrawRect(renderer, &marker);
            }
        }
    }
}

static void draw_overlay(SDL_Renderer *renderer, const Game *game, bool visible) {
    if (!visible) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 5, 8, 12, 170);
    SDL_Rect full = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    SDL_SetRenderDrawColor(renderer, 10, 16, 26, 220);
    SDL_Rect panel = {130, 250, 500, 300};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 80, 102, 130, 255);
    SDL_RenderDrawRect(renderer, &panel);

    set_color(renderer, TEXT);
    const char *title = "GAME OVER";
    const char *label = "SCORE";
    int title_scale = 8;
    int label_scale = 5;
    int title_x = panel.x + (panel.w - text_pixel_width(title_scale, title)) / 2;
    int label_x = panel.x + (panel.w - text_pixel_width(label_scale, label)) / 2;
    draw_text(renderer, title_x, 300, title_scale, title);
    draw_text(renderer, label_x, 390, label_scale, label);

    int score = game->score;
    if (score < 0) {
        score = 0;
    }
    if (score > 9999) {
        score = 9999;
    }
    draw_digit(renderer, 270, 440, 3, (score / 1000) % 10);
    draw_digit(renderer, 332, 440, 3, (score / 100) % 10);
    draw_digit(renderer, 394, 440, 3, (score / 10) % 10);
    draw_digit(renderer, 456, 440, 3, score % 10);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void handle_click(App *app, int x, int y) {
    if (app->turn_anim.active) {
        return;
    }

    if (app->game.game_over) {
        (void)x;
        (void)y;
        game_init(&app->game, (uint32_t)time(NULL));
        sync_render_board(app);
        clear_turn_anim(app);
        clear_particles(app);
        play_tone(app, 640.0f, 100, 0.18f);
        return;
    }

    int board_x = x - BOARD_OFFSET_X;
    int board_y = y - BOARD_OFFSET_Y;
    if (board_x < 0 || board_y < 0) {
        return;
    }

    int col = board_x / CELL_SIZE;
    int row = board_y / CELL_SIZE;
    if (row < 0 || row >= GAME_BOARD_SIZE || col < 0 || col >= GAME_BOARD_SIZE) {
        return;
    }

    uint8_t before[GAME_CELLS];
    memcpy(before, app->game.board, sizeof(before));
    int selected_before = app->game.selected_index;
    int to_idx = rc_to_idx(row, col);
    int path[MAX_PATH_NODES];
    int path_len = 0;
    if (selected_before >= 0 && selected_before < GAME_CELLS && before[to_idx] == 0) {
        (void)build_move_path(before, selected_before, to_idx, path, &path_len);
    }
    int old_score = app->game.score;
    GameAction action = game_click(&app->game, row, col);
    if (action == GAME_ACTION_INVALID) {
        play_tone(app, 140.0f, 60, 0.12f);
    } else if (action == GAME_ACTION_SELECTED) {
        play_tone(app, 300.0f, 40, 0.08f);
    } else if (action == GAME_ACTION_MOVED) {
        start_turn_animation(app, before, selected_before, to_idx, path, path_len);
        if (app->game.score > old_score) {
            play_tone(app, 920.0f, 180, 0.18f);
        } else {
            play_tone(app, 440.0f, 80, 0.10f);
        }
    } else if (action == GAME_ACTION_GAME_OVER) {
        start_turn_animation(app, before, selected_before, to_idx, path, path_len);
        play_tone(app, 200.0f, 260, 0.20f);
    }
}

int main(void) {
    App app;
    if (!app_init(&app)) {
        app_shutdown(&app);
        return 1;
    }

    bool running = true;
    uint32_t prev_ticks = SDL_GetTicks();
    while (running) {
        uint32_t now = SDL_GetTicks();
        float dt = (float)(now - prev_ticks) / 1000.0f;
        if (dt > 0.033f) {
            dt = 0.033f;
        }
        prev_ticks = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                handle_click(&app, event.button.x, event.button.y);
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                game_init(&app.game, (uint32_t)time(NULL));
                sync_render_board(&app);
                clear_turn_anim(&app);
                clear_particles(&app);
                play_tone(&app, 640.0f, 100, 0.18f);
            }
        }
        update_turn_animation(&app, dt);
        update_particles(&app, dt);

        set_color(app.renderer, BG);
        SDL_RenderClear(app.renderer);

        draw_next_balls(app.renderer, &app.game);
        draw_score(app.renderer, app.game.score);
        draw_board(app.renderer, &app);
        draw_move_animation(app.renderer, &app);
        draw_particles(app.renderer, &app);
        draw_overlay(app.renderer, &app.game, app.game.game_over && !app.turn_anim.active);

        SDL_RenderPresent(app.renderer);
    }

    app_shutdown(&app);
    return 0;
}
