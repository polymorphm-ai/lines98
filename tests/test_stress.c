#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "game.h"

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__);                 \
            return 1;                                                                                \
        }                                                                                            \
    } while (0)

static bool has_any_move(const Game *game) {
    for (int fr = 0; fr < GAME_BOARD_SIZE; ++fr) {
        for (int fc = 0; fc < GAME_BOARD_SIZE; ++fc) {
            if (game_get_cell(game, fr, fc) == 0) {
                continue;
            }
            for (int tr = 0; tr < GAME_BOARD_SIZE; ++tr) {
                for (int tc = 0; tc < GAME_BOARD_SIZE; ++tc) {
                    if (game_get_cell(game, tr, tc) != 0) {
                        continue;
                    }
                    if (game_can_reach(game, fr, fc, tr, tc)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static int run_single_game(uint32_t seed, int move_limit) {
    Game game;
    game_init(&game, seed);

    CHECK(game_empty_count(&game) <= GAME_CELLS);

    int moves_done = 0;
    while (!game.game_over && moves_done < move_limit) {
        bool moved = false;

        for (int fr = 0; fr < GAME_BOARD_SIZE && !moved; ++fr) {
            for (int fc = 0; fc < GAME_BOARD_SIZE && !moved; ++fc) {
                if (game_get_cell(&game, fr, fc) == 0) {
                    continue;
                }

                for (int tr = 0; tr < GAME_BOARD_SIZE && !moved; ++tr) {
                    for (int tc = 0; tc < GAME_BOARD_SIZE && !moved; ++tc) {
                        if (game_get_cell(&game, tr, tc) != 0) {
                            continue;
                        }
                        if (!game_can_reach(&game, fr, fc, tr, tc)) {
                            continue;
                        }

                        GameAction a = game_click(&game, fr, fc);
                        CHECK(a == GAME_ACTION_SELECTED);

                        int score_before = game.score;
                        int empty_before = game_empty_count(&game);

                        a = game_click(&game, tr, tc);
                        CHECK(a == GAME_ACTION_MOVED || a == GAME_ACTION_GAME_OVER);

                        int empty_after = game_empty_count(&game);
                        int score_after = game.score;
                        int cleared = score_after - score_before;
                        CHECK(empty_after >= 0 && empty_after <= GAME_CELLS);
                        CHECK(cleared >= 0);

                        /* Net empty cells = empty_before + cleared - spawned, where spawned is 0..3. */
                        CHECK(empty_after <= empty_before + cleared);
                        CHECK(empty_after >= empty_before + cleared - 3);

                        moved = true;
                        ++moves_done;
                    }
                }
            }
        }

        if (!moved) {
            CHECK(game.game_over || !has_any_move(&game));
            break;
        }
    }

    CHECK(game.score >= 0);
    CHECK(game_empty_count(&game) >= 0 && game_empty_count(&game) <= GAME_CELLS);

    return 0;
}

int main(void) {
    const int games = 40;
    const int move_limit = 500;

    for (int i = 0; i < games; ++i) {
        uint32_t seed = 1000u + (uint32_t)i * 7919u;
        if (run_single_game(seed, move_limit) != 0) {
            return 1;
        }
    }

    printf("Stress tests passed (%d games, up to %d moves each).\n", games, move_limit);
    return 0;
}
