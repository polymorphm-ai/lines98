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

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    bool audio_ready;
    Game game;
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

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    set_color(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        int span = (int)sqrt((double)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
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
        draw_filled_circle(renderer, x, y, 18, BALL_COLORS[game->next_colors[i]]);
    }
}

static void draw_board(SDL_Renderer *renderer, const Game *game) {
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
    if (game->selected_index >= 0) {
        sel_row = game->selected_index / GAME_BOARD_SIZE;
        sel_col = game->selected_index % GAME_BOARD_SIZE;
    }

    for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
        for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
            uint8_t cell = game_get_cell(game, row, col);
            if (cell == 0) {
                continue;
            }

            int cx = BOARD_OFFSET_X + col * CELL_SIZE + CELL_SIZE / 2;
            int cy = BOARD_OFFSET_Y + row * CELL_SIZE + CELL_SIZE / 2;
            draw_filled_circle(renderer, cx, cy, 23, BALL_COLORS[cell]);

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
    SDL_Rect panel = {150, 250, 460, 300};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 80, 102, 130, 255);
    SDL_RenderDrawRect(renderer, &panel);

    set_color(renderer, TEXT);
    draw_text(renderer, 210, 300, 8, "GAME OVER");
    draw_text(renderer, 280, 390, 5, "SCORE");

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

    int old_score = app->game.score;
    GameAction action = game_click(&app->game, row, col);
    if (action == GAME_ACTION_INVALID) {
        play_tone(app, 140.0f, 60, 0.12f);
    } else if (action == GAME_ACTION_SELECTED) {
        play_tone(app, 300.0f, 40, 0.08f);
    } else if (action == GAME_ACTION_MOVED) {
        if (app->game.score > old_score) {
            play_tone(app, 920.0f, 180, 0.18f);
        } else {
            play_tone(app, 440.0f, 80, 0.10f);
        }
    } else if (action == GAME_ACTION_GAME_OVER) {
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
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                handle_click(&app, event.button.x, event.button.y);
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                game_init(&app.game, (uint32_t)time(NULL));
                play_tone(&app, 640.0f, 100, 0.18f);
            }
        }

        set_color(app.renderer, BG);
        SDL_RenderClear(app.renderer);

        draw_next_balls(app.renderer, &app.game);
        draw_score(app.renderer, app.game.score);
        draw_board(app.renderer, &app.game);
        draw_overlay(app.renderer, &app.game, app.game.game_over);

        SDL_RenderPresent(app.renderer);
    }

    app_shutdown(&app);
    return 0;
}
