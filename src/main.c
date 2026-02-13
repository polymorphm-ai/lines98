/* SDL frontend for Lines-98.
   It owns rendering, audio, input handling and visual turn effects. */

#include <SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fx_particles.h"
#include "game.h"
#include "render_ui.h"
#include "turn_controller.h"
#include "turn_anim.h"

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

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    bool audio_ready;
    Game game;
    uint8_t render_board[GAME_CELLS];
    TurnAnim turn_anim;
    ParticleSystem particles;
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

/* Returns pixel X coordinate of board cell center. */
static int ball_center_x(int col) {
    return BOARD_OFFSET_X + col * CELL_SIZE + CELL_SIZE / 2;
}

/* Returns pixel Y coordinate of board cell center. */
static int ball_center_y(int row) {
    return BOARD_OFFSET_Y + row * CELL_SIZE + CELL_SIZE / 2;
}

/* Converts linear index to board row/col. */
static bool idx_to_rc(int idx, int *row, int *col) {
    if (idx < 0 || idx >= GAME_CELLS) {
        return false;
    }
    *row = idx / GAME_BOARD_SIZE;
    *col = idx % GAME_BOARD_SIZE;
    return true;
}

/* Copies authoritative game board into render board. */
static void sync_render_board(App *app) {
    memcpy(app->render_board, app->game.board, sizeof(app->render_board));
}

/* Resets turn animation state to idle. */
static void clear_turn_anim(App *app) {
    turn_anim_init(&app->turn_anim);
}

/* Clears all active dust particles. */
static void clear_particles(App *app) {
    particles_init(&app->particles);
}

/* Emits dust burst for all cleared balls in current turn animation. */
static void emit_clear_particles(App *app, const TurnAnim *anim) {
    int count = turn_anim_cleared_count(anim);
    for (int i = 0; i < count; ++i) {
        int idx = turn_anim_cleared_idx(anim, i);
        int row;
        int col;
        if (!idx_to_rc(idx, &row, &col)) {
            continue;
        }

        float x = (float)ball_center_x(col);
        float y = (float)ball_center_y(row);
        SDL_Color color = BALL_COLORS[turn_anim_cleared_color(anim, i)];
        particles_spawn_burst(&app->particles, x, y, color, 18);
    }
}

/* Starts queued turn animation from pre/post board snapshots. */
static void start_turn_animation(
    App *app,
    const uint8_t *before,
    int from_idx,
    int to_idx,
    const int *path,
    int path_len
) {
    turn_anim_start(&app->turn_anim, before, app->game.board, from_idx, to_idx, path, path_len);
    turn_anim_begin_render(&app->turn_anim, app->render_board, GAME_CELLS);
}

/* Returns growth scale for spawned ball or -1 when not in spawn phase. */
static float spawned_scale_for_index(const TurnAnim *anim, int idx) {
    return turn_anim_spawn_scale_for_index(anim, idx);
}

/* Advances turn animation queue and triggers clear burst handoff. */
static void update_turn_animation(App *app, float dt) {
    bool emit = false;
    turn_anim_update(&app->turn_anim, dt, app->render_board, GAME_CELLS, &emit);
    if (emit) {
        emit_clear_particles(app, &app->turn_anim);
    }
}

/* Advances particle simulation with board collisions. */
static void update_particles(App *app, float dt) {
    particles_update(&app->particles, dt, app->render_board, GAME_BOARD_SIZE, CELL_SIZE, BOARD_OFFSET_X, BOARD_OFFSET_Y, 23.0f);
}

/* Draws all active particles with alpha fade. */
static void draw_particles(SDL_Renderer *renderer, const App *app) {
    particles_draw(renderer, &app->particles);
}

/* Draws interpolated moving ball during MOVE phase. */
static void draw_move_animation(SDL_Renderer *renderer, const App *app) {
    const TurnAnim *anim = &app->turn_anim;
    if (!anim->active || anim->move.color == 0 || anim->move.path_len < 2) {
        return;
    }

    float u = 0.0f;
    if (!turn_anim_move_u(anim, &u)) {
        return;
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
    ru_draw_ball(renderer, cx, cy, 23, BALL_COLORS[anim->move.color]);
}

/* Renders top-right score display. */
static void draw_score(SDL_Renderer *renderer, int score) {
    if (score < 0) {
        score = 0;
    }
    if (score > 9999) {
        score = 9999;
    }

    ru_set_color(renderer, TEXT);
    int d0 = (score / 1000) % 10;
    int d1 = (score / 100) % 10;
    int d2 = (score / 10) % 10;
    int d3 = score % 10;

    ru_draw_digit(renderer, 520, 18, 2, d0);
    ru_draw_digit(renderer, 560, 18, 2, d1);
    ru_draw_digit(renderer, 600, 18, 2, d2);
    ru_draw_digit(renderer, 640, 18, 2, d3);
}

/* Generates and queues a short synthesized tone. */
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

/* Initializes SDL systems, window, renderer, audio and game state. */
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

/* Releases all SDL resources owned by the app. */
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

/* Draws preview balls for the next spawn step. */
static void draw_next_balls(SDL_Renderer *renderer, const Game *game) {
    for (int i = 0; i < GAME_NEXT_COUNT; ++i) {
        int x = NEXT_OFFSET_X + i * 56;
        int y = NEXT_OFFSET_Y;
        ru_draw_ball(renderer, x, y, 18, BALL_COLORS[game->next_colors[i]]);
    }
}

/* Draws board grid and all balls from current render snapshot. */
static void draw_board(SDL_Renderer *renderer, const App *app) {
    SDL_Rect board_rect = {BOARD_OFFSET_X, BOARD_OFFSET_Y, GAME_BOARD_SIZE * CELL_SIZE, GAME_BOARD_SIZE * CELL_SIZE};
    ru_set_color(renderer, GRID_BG);
    SDL_RenderFillRect(renderer, &board_rect);

    ru_set_color(renderer, GRID_LINE);
    for (int i = 0; i <= GAME_BOARD_SIZE; ++i) {
        int x = BOARD_OFFSET_X + i * CELL_SIZE;
        int y = BOARD_OFFSET_Y + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, x, BOARD_OFFSET_Y, x, BOARD_OFFSET_Y + GAME_BOARD_SIZE * CELL_SIZE);
        SDL_RenderDrawLine(renderer, BOARD_OFFSET_X, y, BOARD_OFFSET_X + GAME_BOARD_SIZE * CELL_SIZE, y);
    }

    int sel_row = -1;
    int sel_col = -1;
    if (!turn_anim_active(&app->turn_anim) && app->game.selected_index >= 0) {
        sel_row = app->game.selected_index / GAME_BOARD_SIZE;
        sel_col = app->game.selected_index % GAME_BOARD_SIZE;
    }

    for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
        for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
            int idx = row * GAME_BOARD_SIZE + col;
            uint8_t cell = turn_anim_active(&app->turn_anim) ? app->render_board[idx] : app->game.board[idx];
            if (cell == 0) {
                continue;
            }

            int cx = ball_center_x(col);
            int cy = ball_center_y(row);
            int radius = 23;
            if (turn_anim_active(&app->turn_anim)) {
                float s = spawned_scale_for_index(&app->turn_anim, idx);
                if (s >= 0.0f) {
                    radius = (int)(2.0f + 21.0f * s);
                }
            }
            ru_draw_ball(renderer, cx, cy, radius, BALL_COLORS[cell]);

            if (row == sel_row && col == sel_col) {
                ru_set_color(renderer, SELECTED);
                SDL_Rect marker = {cx - 26, cy - 26, 52, 52};
                SDL_RenderDrawRect(renderer, &marker);
            }
        }
    }
}

/* Draws game-over overlay panel and final score. */
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

    ru_set_color(renderer, TEXT);
    const char *title = "GAME OVER";
    const char *label = "SCORE";
    int title_scale = 8;
    int label_scale = 5;
    int title_x = panel.x + (panel.w - ru_text_pixel_width(title_scale, title)) / 2;
    int label_x = panel.x + (panel.w - ru_text_pixel_width(label_scale, label)) / 2;
    ru_draw_text(renderer, title_x, 300, title_scale, title);
    ru_draw_text(renderer, label_x, 390, label_scale, label);

    int score = game->score;
    if (score < 0) {
        score = 0;
    }
    if (score > 9999) {
        score = 9999;
    }
    ru_draw_digit(renderer, 270, 440, 3, (score / 1000) % 10);
    ru_draw_digit(renderer, 332, 440, 3, (score / 100) % 10);
    ru_draw_digit(renderer, 394, 440, 3, (score / 10) % 10);
    ru_draw_digit(renderer, 456, 440, 3, score % 10);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* Handles left click input with selection/move/restart rules. */
static void handle_click(App *app, int x, int y) {
    if (turn_anim_active(&app->turn_anim)) {
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

    TurnClickResult result;
    turn_controller_click(&app->game, row, col, &result);
    int old_score = result.score_before;
    GameAction action = result.action;
    if (action == GAME_ACTION_INVALID) {
        play_tone(app, 140.0f, 60, 0.12f);
    } else if (action == GAME_ACTION_SELECTED) {
        play_tone(app, 300.0f, 40, 0.08f);
    } else if (action == GAME_ACTION_MOVED) {
        start_turn_animation(app, result.before_board, result.from_idx, result.to_idx, result.path, result.path_len);
        if (app->game.score > old_score) {
            play_tone(app, 920.0f, 180, 0.18f);
        } else {
            play_tone(app, 440.0f, 80, 0.10f);
        }
    } else if (action == GAME_ACTION_GAME_OVER) {
        start_turn_animation(app, result.before_board, result.from_idx, result.to_idx, result.path, result.path_len);
        play_tone(app, 200.0f, 260, 0.20f);
    }
}

/* Runs SDL event loop, frame updates and rendering. */
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

        ru_set_color(app.renderer, BG);
        SDL_RenderClear(app.renderer);

        draw_next_balls(app.renderer, &app.game);
        draw_score(app.renderer, app.game.score);
        draw_board(app.renderer, &app);
        draw_move_animation(app.renderer, &app);
        draw_particles(app.renderer, &app);
        draw_overlay(app.renderer, &app.game, app.game.game_over && !turn_anim_active(&app.turn_anim));

        SDL_RenderPresent(app.renderer);
    }

    app_shutdown(&app);
    return 0;
}
