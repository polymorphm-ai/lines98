/* Turn controller for board clicks.
   It bridges game rules and UI animation metadata while staying SDL-free. */

#include "turn_controller.h"

#include <string.h>

/* Converts board row/col to linear index. */
static int rc_to_idx(int row, int col) {
    return row * GAME_BOARD_SIZE + col;
}

/* Converts linear index to board row/col. */
static void idx_to_rc(int idx, int *row, int *col) {
    *row = idx / GAME_BOARD_SIZE;
    *col = idx % GAME_BOARD_SIZE;
}

/* Checks whether coordinates are inside board bounds. */
static bool in_bounds(int row, int col) {
    return row >= 0 && row < GAME_BOARD_SIZE && col >= 0 && col < GAME_BOARD_SIZE;
}

/* Builds shortest path for move animation on pre-click board snapshot. */
static bool build_move_path(const uint8_t *board, int from_idx, int to_idx, int *path, int *path_len) {
    if (from_idx < 0 || from_idx >= GAME_CELLS || to_idx < 0 || to_idx >= GAME_CELLS) {
        return false;
    }
    if (from_idx == to_idx) {
        path[0] = from_idx;
        *path_len = 1;
        return true;
    }

    int prev[GAME_CELLS];
    for (int i = 0; i < GAME_CELLS; ++i) {
        prev[i] = -1;
    }

    int queue[GAME_CELLS];
    int head = 0;
    int tail = 0;
    queue[tail++] = from_idx;
    prev[from_idx] = from_idx;

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    bool found = false;

    while (head < tail && !found) {
        int cur = queue[head++];
        int row;
        int col;
        idx_to_rc(cur, &row, &col);

        for (int k = 0; k < 4; ++k) {
            int nr = row + dr[k];
            int nc = col + dc[k];
            if (!in_bounds(nr, nc)) {
                continue;
            }

            int nxt = rc_to_idx(nr, nc);
            if (prev[nxt] != -1) {
                continue;
            }
            if (nxt != to_idx && board[nxt] != 0) {
                continue;
            }

            prev[nxt] = cur;
            if (nxt == to_idx) {
                found = true;
                break;
            }
            queue[tail++] = nxt;
        }
    }

    if (!found) {
        return false;
    }

    int rev[TC_MAX_PATH_NODES];
    int n = 0;
    for (int cur = to_idx; cur != from_idx; cur = prev[cur]) {
        rev[n++] = cur;
    }
    rev[n++] = from_idx;

    for (int i = 0; i < n; ++i) {
        path[i] = rev[n - 1 - i];
    }
    *path_len = n;
    return true;
}

/* Processes one board click and prepares animation metadata if a move happened. */
void turn_controller_click(Game *game, int row, int col, TurnClickResult *out) {
    memset(out, 0, sizeof(*out));
    out->from_idx = -1;
    out->to_idx = -1;
    out->action = GAME_ACTION_INVALID;

    if (row < 0 || row >= GAME_BOARD_SIZE || col < 0 || col >= GAME_BOARD_SIZE) {
        return;
    }

    memcpy(out->before_board, game->board, sizeof(out->before_board));
    out->score_before = game->score;

    int selected_before = game->selected_index;
    out->to_idx = rc_to_idx(row, col);

    if (selected_before >= 0 && selected_before < GAME_CELLS && out->before_board[out->to_idx] == 0) {
        out->from_idx = selected_before;
        (void)build_move_path(out->before_board, out->from_idx, out->to_idx, out->path, &out->path_len);
    }

    out->action = game_click(game, row, col);
    out->score_after = game->score;
    out->has_move_animation = (out->action == GAME_ACTION_MOVED || out->action == GAME_ACTION_GAME_OVER);
}
