#include <stdio.h>
#include <string.h>

#include "turn_controller.h"

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__);                 \
            return 1;                                                                                \
        }                                                                                            \
    } while (0)

static void clear_board(Game *g) {
    memset(g->board, 0, sizeof(g->board));
    g->selected_index = -1;
    g->score = 0;
    g->game_over = false;
}

static int test_select_only(void) {
    Game g;
    game_init(&g, 1);
    clear_board(&g);
    g.board[0] = 3;

    TurnClickResult r;
    turn_controller_click(&g, 0, 0, &r);

    CHECK(r.action == GAME_ACTION_SELECTED);
    CHECK(r.has_move_animation == false);
    CHECK(g.selected_index == 0);
    return 0;
}

static int test_move_with_path(void) {
    Game g;
    game_init(&g, 2);
    clear_board(&g);
    g.board[0] = 2;
    g.selected_index = 0;

    TurnClickResult r;
    turn_controller_click(&g, 0, 1, &r);

    CHECK(r.action == GAME_ACTION_MOVED || r.action == GAME_ACTION_GAME_OVER);
    CHECK(r.has_move_animation == true);
    CHECK(r.from_idx == 0);
    CHECK(r.to_idx == 1);
    CHECK(r.path_len >= 2);
    CHECK(r.path[0] == 0);
    CHECK(r.path[r.path_len - 1] == 1);
    CHECK(r.before_board[0] == 2);
    CHECK(r.before_board[1] == 0);
    CHECK(r.score_after >= r.score_before);
    return 0;
}

static int test_invalid_when_no_selection(void) {
    Game g;
    game_init(&g, 3);
    clear_board(&g);

    TurnClickResult r;
    turn_controller_click(&g, 4, 4, &r);

    CHECK(r.action == GAME_ACTION_INVALID);
    CHECK(r.has_move_animation == false);
    CHECK(r.path_len == 0);
    return 0;
}

int main(void) {
    if (test_select_only() != 0) {
        return 1;
    }
    if (test_move_with_path() != 0) {
        return 1;
    }
    if (test_invalid_when_no_selection() != 0) {
        return 1;
    }

    printf("Turn controller tests passed.\n");
    return 0;
}
