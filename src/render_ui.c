/* Reusable SDL rendering primitives for Lines-98 UI.
   Keeps drawing details out of main loop code. */

#include "render_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* Scales RGB channels by scalar factor. */
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

/* Blends base color towards white by t in [0..1]. */
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

/* Draws one seven-segment rectangle slice when enabled. */
static void draw_segment(SDL_Renderer *renderer, int x, int y, int w, int h, bool on) {
    if (!on) {
        return;
    }
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

/* Draws one small bitmap glyph used in overlays. */
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
        case 'A': glyph = glyph_a; break;
        case 'C': glyph = glyph_c; break;
        case 'E': glyph = glyph_e; break;
        case 'G': glyph = glyph_g; break;
        case 'M': glyph = glyph_m; break;
        case 'O': glyph = glyph_o; break;
        case 'R': glyph = glyph_r; break;
        case 'S': glyph = glyph_s; break;
        case 'V': glyph = glyph_v; break;
        default: glyph = glyph_space; break;
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

/* Sets current renderer draw color. */
void ru_set_color(SDL_Renderer *renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

/* Draws pseudo-3D game ball with highlight/shadow. */
void ru_draw_ball(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color base) {
    draw_filled_circle(renderer, cx + 2, cy + 3, radius, scale_color(base, 0.28f));
    draw_filled_circle(renderer, cx, cy, radius, scale_color(base, 0.72f));
    draw_filled_circle(renderer, cx - 1, cy - 1, (radius * 8) / 10, base);
    draw_filled_circle(renderer, cx - 5, cy - 6, radius / 2, mix_white(base, 0.65f));
    draw_filled_circle(renderer, cx - 8, cy - 9, SDL_max(2, radius / 6), mix_white(base, 0.9f));
}

/* Draws one seven-segment digit. */
void ru_draw_digit(SDL_Renderer *renderer, int x, int y, int scale, int digit) {
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

/* Draws text with tiny bitmap glyphs. */
void ru_draw_text(SDL_Renderer *renderer, int x, int y, int scale, const char *text) {
    int cursor = x;
    for (const char *p = text; *p != '\0'; ++p) {
        draw_glyph(renderer, cursor, y, scale, *p);
        cursor += 6 * scale;
    }
}

/* Returns pixel width of text drawn by ru_draw_text at given scale. */
int ru_text_pixel_width(int scale, const char *text) {
    int len = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        ++len;
    }
    return len * 6 * scale;
}
