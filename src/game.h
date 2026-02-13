#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rng.h"

#define GAME_BOARD_SIZE 9
#define GAME_CELLS (GAME_BOARD_SIZE * GAME_BOARD_SIZE)
#define GAME_NEXT_COUNT 3
#define GAME_COLORS 7

typedef enum {
    GAME_ACTION_NONE = 0,
    GAME_ACTION_SELECTED = 1,
    GAME_ACTION_MOVED = 2,
    GAME_ACTION_INVALID = 3,
    GAME_ACTION_GAME_OVER = 4
} GameAction;

typedef struct {
    uint8_t board[GAME_CELLS];
    uint8_t next_colors[GAME_NEXT_COUNT];
    int selected_index;
    int score;
    bool game_over;
    Rng rng;
} Game;

void game_init(Game *game, uint32_t seed);
uint8_t game_get_cell(const Game *game, int row, int col);
bool game_can_reach(const Game *game, int from_row, int from_col, int to_row, int to_col);
GameAction game_click(Game *game, int row, int col);
int game_empty_count(const Game *game);

#endif
