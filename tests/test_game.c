#include <stdio.h>
#include <string.h>

#include "game.h"

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__);                 \
            return 1;                                                                                \
        }                                                                                            \
    } while (0)

static void clear_board(Game *game) {
    memset(game->board, 0, sizeof(game->board));
    game->selected_index = -1;
    game->score = 0;
    game->game_over = false;
}

static int test_init(void) {
    Game game;
    game_init(&game, 1234);

    CHECK(game_empty_count(&game) == GAME_CELLS - 5);
    CHECK(game.score == 0);
    CHECK(game.game_over == false);
    for (int i = 0; i < GAME_NEXT_COUNT; ++i) {
        CHECK(game.next_colors[i] >= 1 && game.next_colors[i] <= GAME_COLORS);
    }
    return 0;
}

static int test_pathfinding(void) {
    Game game;
    game_init(&game, 42);
    clear_board(&game);

    game.board[0] = 1;
    for (int i = 1; i < GAME_BOARD_SIZE; ++i) {
        game.board[i] = 2;
    }

    CHECK(game_can_reach(&game, 0, 0, 1, 0) == true);
    CHECK(game_can_reach(&game, 0, 0, 0, 8) == false);

    game.board[9] = 2;
    CHECK(game_can_reach(&game, 0, 0, 1, 0) == false);

    return 0;
}

static int test_line_clear_after_move(void) {
    Game game;
    game_init(&game, 11);
    clear_board(&game);

    game.board[1 * GAME_BOARD_SIZE + 0] = 3;
    game.board[1 * GAME_BOARD_SIZE + 1] = 3;
    game.board[1 * GAME_BOARD_SIZE + 2] = 3;
    game.board[1 * GAME_BOARD_SIZE + 3] = 3;
    game.board[0 * GAME_BOARD_SIZE + 4] = 3;

    GameAction a = game_click(&game, 0, 4);
    CHECK(a == GAME_ACTION_SELECTED);
    a = game_click(&game, 1, 4);
    CHECK(a == GAME_ACTION_MOVED);

    for (int c = 0; c < 5; ++c) {
        CHECK(game.board[1 * GAME_BOARD_SIZE + c] == 0);
    }
    CHECK(game.score == 10);
    CHECK(game_empty_count(&game) == GAME_CELLS);

    return 0;
}

static int test_spawn_and_game_over(void) {
    Game game;
    game_init(&game, 77);
    clear_board(&game);

    for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
        for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
            int idx = row * GAME_BOARD_SIZE + col;
            game.board[idx] = (uint8_t)(((row * 2 + col * 3) % GAME_COLORS) + 1);
        }
    }

    game.board[0] = 2;
    game.board[1] = 0;
    game.next_colors[0] = 4;
    game.next_colors[1] = 5;
    game.next_colors[2] = 6;

    GameAction a = game_click(&game, 0, 0);
    CHECK(a == GAME_ACTION_SELECTED);
    a = game_click(&game, 0, 1);
    CHECK(a == GAME_ACTION_GAME_OVER);
    CHECK(game.game_over == true);
    CHECK(game_empty_count(&game) == 0);

    return 0;
}

static int test_multi_line_single_move_scoring(void) {
    Game game;
    game_init(&game, 99);
    clear_board(&game);

    /* Prepare a board where one move closes both a vertical (9) and a horizontal (5)
       line through the center cell. Total unique cleared balls = 13. */
    for (int r = 0; r < GAME_BOARD_SIZE; ++r) {
        if (r != 4) {
            game.board[r * GAME_BOARD_SIZE + 4] = 2;
        }
    }
    for (int c = 5; c < GAME_BOARD_SIZE; ++c) {
        game.board[4 * GAME_BOARD_SIZE + c] = 2;
    }
    game.board[4 * GAME_BOARD_SIZE + 2] = 2;

    GameAction a = game_click(&game, 4, 2);
    CHECK(a == GAME_ACTION_SELECTED);
    a = game_click(&game, 4, 4);
    CHECK(a == GAME_ACTION_MOVED);

    CHECK(game_empty_count(&game) == GAME_CELLS);
    CHECK(game.score == 138);
    CHECK(game.game_over == false);

    return 0;
}

int main(void) {
    if (test_init() != 0) {
        return 1;
    }
    if (test_pathfinding() != 0) {
        return 1;
    }
    if (test_line_clear_after_move() != 0) {
        return 1;
    }
    if (test_spawn_and_game_over() != 0) {
        return 1;
    }
    if (test_multi_line_single_move_scoring() != 0) {
        return 1;
    }

    printf("All tests passed.\n");
    return 0;
}
