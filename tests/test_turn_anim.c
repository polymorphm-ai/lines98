#include <stdio.h>
#include <string.h>

#include "game.h"
#include "turn_anim.h"

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__);                 \
            return 1;                                                                                \
        }                                                                                            \
    } while (0)

static int test_move_only_sequence(void) {
    TurnAnim anim;
    uint8_t before[GAME_CELLS] = {0};
    uint8_t final_board[GAME_CELLS] = {0};
    uint8_t render[GAME_CELLS] = {0};

    int from = 0;
    int to = 1;
    before[from] = 3;
    final_board[to] = 3;
    int path[2] = {from, to};

    turn_anim_start(&anim, before, final_board, from, to, path, 2);
    CHECK(turn_anim_active(&anim));

    turn_anim_begin_render(&anim, render, GAME_CELLS);
    CHECK(render[from] == 0);
    CHECK(render[to] == 0);

    bool emit = false;
    turn_anim_update(&anim, 0.19f, render, GAME_CELLS, &emit);
    CHECK(emit == true);
    CHECK(turn_anim_spawn_scale_for_index(&anim, to) == -1.0f);
    CHECK(render[to] == 3);

    turn_anim_update(&anim, 0.19f, render, GAME_CELLS, &emit);
    CHECK(turn_anim_active(&anim) == false);
    CHECK(render[to] == 3);

    return 0;
}

static int test_move_clear_spawn_sequence(void) {
    TurnAnim anim;
    uint8_t before[GAME_CELLS] = {0};
    uint8_t final_board[GAME_CELLS] = {0};
    uint8_t render[GAME_CELLS] = {0};

    int from = 9;
    int to = 0;
    before[from] = 1;
    before[1] = 1;
    before[2] = 1;
    before[3] = 1;
    before[4] = 1;
    final_board[80] = 2;

    int path[2] = {from, to};
    turn_anim_start(&anim, before, final_board, from, to, path, 2);
    turn_anim_begin_render(&anim, render, GAME_CELLS);

    bool emit = false;
    turn_anim_update(&anim, 0.19f, render, GAME_CELLS, &emit);
    CHECK(emit == true);
    CHECK(turn_anim_cleared_count(&anim) >= 5);
    CHECK(render[0] == 0);
    CHECK(render[1] == 0);
    CHECK(render[2] == 0);
    CHECK(render[3] == 0);
    CHECK(render[4] == 0);
    CHECK(render[80] == 0);

    turn_anim_update(&anim, 0.17f, render, GAME_CELLS, &emit);
    CHECK(turn_anim_spawn_scale_for_index(&anim, 80) == 0.0f);
    CHECK(render[80] == 0);

    turn_anim_update(&anim, 0.01f, render, GAME_CELLS, &emit);
    CHECK(turn_anim_spawn_scale_for_index(&anim, 80) > 0.0f);
    CHECK(render[80] == 2);

    turn_anim_update(&anim, 0.20f, render, GAME_CELLS, &emit);
    CHECK(turn_anim_active(&anim) == false);
    CHECK(render[80] == 2);

    return 0;
}

int main(void) {
    if (test_move_only_sequence() != 0) {
        return 1;
    }
    if (test_move_clear_spawn_sequence() != 0) {
        return 1;
    }

    printf("Turn animation tests passed.\n");
    return 0;
}
