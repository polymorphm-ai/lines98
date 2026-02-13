#ifndef TURN_CONTROLLER_H
#define TURN_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

#define TC_MAX_PATH_NODES GAME_CELLS

/* Result payload for one click processed by turn controller. */
typedef struct {
    GameAction action;
    bool has_move_animation;
    int from_idx;
    int to_idx;
    int path[TC_MAX_PATH_NODES];
    int path_len;
    int score_before;
    int score_after;
    uint8_t before_board[GAME_CELLS];
} TurnClickResult;

/* Processes one board click and prepares animation metadata if a move happened. */
void turn_controller_click(Game *game, int row, int col, TurnClickResult *out);

#endif
