#ifndef RENDER_UI_H
#define RENDER_UI_H

#include <SDL2/SDL.h>

/* Sets current renderer draw color. */
void ru_set_color(SDL_Renderer *renderer, SDL_Color color);

/* Draws pseudo-3D game ball with highlight/shadow. */
void ru_draw_ball(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color base);

/* Draws one seven-segment digit. */
void ru_draw_digit(SDL_Renderer *renderer, int x, int y, int scale, int digit);

/* Draws text with tiny bitmap glyphs. */
void ru_draw_text(SDL_Renderer *renderer, int x, int y, int scale, const char *text);

/* Returns pixel width of text drawn by ru_draw_text at given scale. */
int ru_text_pixel_width(int scale, const char *text);

#endif
