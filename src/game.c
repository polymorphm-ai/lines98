#include "game.h"

#include <string.h>

static bool in_bounds(int row, int col) {
    return row >= 0 && row < GAME_BOARD_SIZE && col >= 0 && col < GAME_BOARD_SIZE;
}

static int to_index(int row, int col) {
    return row * GAME_BOARD_SIZE + col;
}

static void to_row_col(int idx, int *row, int *col) {
    *row = idx / GAME_BOARD_SIZE;
    *col = idx % GAME_BOARD_SIZE;
}

static int generate_color(Game *game) {
    return (int)rng_range(&game->rng, GAME_COLORS) + 1;
}

static void generate_next(Game *game) {
    for (int i = 0; i < GAME_NEXT_COUNT; ++i) {
        game->next_colors[i] = (uint8_t)generate_color(game);
    }
}

int game_empty_count(const Game *game) {
    int count = 0;
    for (int i = 0; i < GAME_CELLS; ++i) {
        if (game->board[i] == 0) {
            ++count;
        }
    }
    return count;
}

static int spawn_random_balls(Game *game, const uint8_t *colors, int count) {
    int empties[GAME_CELLS];
    int empty_count = 0;

    for (int i = 0; i < GAME_CELLS; ++i) {
        if (game->board[i] == 0) {
            empties[empty_count++] = i;
        }
    }

    int placed = 0;
    for (int i = 0; i < count && empty_count > 0; ++i) {
        int pick = (int)rng_range(&game->rng, (uint32_t)empty_count);
        int idx = empties[pick];
        game->board[idx] = colors[i];
        empties[pick] = empties[empty_count - 1];
        --empty_count;
        ++placed;
    }
    return placed;
}

static int clear_lines(Game *game) {
    static const int dirs[4][2] = {
        {1, 0},
        {0, 1},
        {1, 1},
        {1, -1}
    };

    bool to_clear[GAME_CELLS];
    memset(to_clear, 0, sizeof(to_clear));

    for (int row = 0; row < GAME_BOARD_SIZE; ++row) {
        for (int col = 0; col < GAME_BOARD_SIZE; ++col) {
            uint8_t color = game->board[to_index(row, col)];
            if (color == 0) {
                continue;
            }

            for (int d = 0; d < 4; ++d) {
                int dr = dirs[d][0];
                int dc = dirs[d][1];

                int prev_r = row - dr;
                int prev_c = col - dc;
                if (in_bounds(prev_r, prev_c) && game->board[to_index(prev_r, prev_c)] == color) {
                    continue;
                }

                int length = 0;
                int r = row;
                int c = col;
                while (in_bounds(r, c) && game->board[to_index(r, c)] == color) {
                    ++length;
                    r += dr;
                    c += dc;
                }

                if (length >= 5) {
                    r = row;
                    c = col;
                    for (int i = 0; i < length; ++i) {
                        to_clear[to_index(r, c)] = true;
                        r += dr;
                        c += dc;
                    }
                }
            }
        }
    }

    int cleared = 0;
    for (int i = 0; i < GAME_CELLS; ++i) {
        if (to_clear[i]) {
            game->board[i] = 0;
            ++cleared;
        }
    }

    if (cleared >= 5) {
        /* Canonical Lines-98 progression:
           5->10, 6->12, 7->18, 8->28, 9->42.
           It continues naturally as score = 2 * (n - 5)^2 + 10. */
        int d = cleared - 5;
        game->score += 2 * d * d + 10;
    }
    return cleared;
}

static bool finish_turn(Game *game) {
    int cleared = clear_lines(game);
    if (cleared == 0) {
        (void)spawn_random_balls(game, game->next_colors, GAME_NEXT_COUNT);
        (void)clear_lines(game);
    }

    generate_next(game);
    game->selected_index = -1;

    if (game_empty_count(game) == 0) {
        game->game_over = true;
        return true;
    }
    return false;
}

void game_init(Game *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    rng_seed(&game->rng, seed);
    game->selected_index = -1;
    game->score = 0;
    game->game_over = false;

    generate_next(game);

    uint8_t initial[5];
    for (int i = 0; i < 5; ++i) {
        initial[i] = (uint8_t)generate_color(game);
    }
    (void)spawn_random_balls(game, initial, 5);
}

uint8_t game_get_cell(const Game *game, int row, int col) {
    if (!in_bounds(row, col)) {
        return 0;
    }
    return game->board[to_index(row, col)];
}

bool game_can_reach(const Game *game, int from_row, int from_col, int to_row, int to_col) {
    if (!in_bounds(from_row, from_col) || !in_bounds(to_row, to_col)) {
        return false;
    }

    int from = to_index(from_row, from_col);
    int to = to_index(to_row, to_col);

    if (from == to) {
        return true;
    }
    if (game->board[from] == 0 || game->board[to] != 0) {
        return false;
    }

    int queue[GAME_CELLS];
    int head = 0;
    int tail = 0;
    bool visited[GAME_CELLS];
    memset(visited, 0, sizeof(visited));

    queue[tail++] = from;
    visited[from] = true;

    static const int dr[4] = {-1, 1, 0, 0};
    static const int dc[4] = {0, 0, -1, 1};

    while (head < tail) {
        int cur = queue[head++];
        int row;
        int col;
        to_row_col(cur, &row, &col);

        for (int i = 0; i < 4; ++i) {
            int nr = row + dr[i];
            int nc = col + dc[i];
            if (!in_bounds(nr, nc)) {
                continue;
            }

            int next = to_index(nr, nc);
            if (visited[next]) {
                continue;
            }

            if (next == to) {
                return true;
            }

            if (game->board[next] == 0) {
                visited[next] = true;
                queue[tail++] = next;
            }
        }
    }

    return false;
}

GameAction game_click(Game *game, int row, int col) {
    if (!in_bounds(row, col) || game->game_over) {
        return GAME_ACTION_INVALID;
    }

    int idx = to_index(row, col);
    uint8_t cell = game->board[idx];

    if (cell != 0) {
        game->selected_index = idx;
        return GAME_ACTION_SELECTED;
    }

    if (game->selected_index < 0) {
        return GAME_ACTION_INVALID;
    }

    int from_row;
    int from_col;
    to_row_col(game->selected_index, &from_row, &from_col);
    if (!game_can_reach(game, from_row, from_col, row, col)) {
        return GAME_ACTION_INVALID;
    }

    game->board[idx] = game->board[game->selected_index];
    game->board[game->selected_index] = 0;

    bool over = finish_turn(game);
    return over ? GAME_ACTION_GAME_OVER : GAME_ACTION_MOVED;
}
