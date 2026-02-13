/* Turn animation queue/state-machine.
   This file keeps phase order move -> clear -> spawn and owns board snapshots
   so rendering can stay consistent until animation fully completes. */

#include "turn_anim.h"

#include <string.h>

/* Clamps path length to valid static storage range. */
static int clamp_path_len(int path_len) {
    if (path_len < 0) {
        return 0;
    }
    if (path_len > TA_MAX_PATH_NODES) {
        return TA_MAX_PATH_NODES;
    }
    return path_len;
}

/* Copies fixed-size board snapshot. */
static void copy_board(uint8_t *dst, const uint8_t *src) {
    memcpy(dst, src, GAME_CELLS);
}

/* Checks whether an index is inside board linear range. */
static bool idx_ok(int idx) {
    return idx >= 0 && idx < GAME_CELLS;
}

/* Resets animation state to idle. */
void turn_anim_init(TurnAnim *anim) {
    memset(anim, 0, sizeof(*anim));
}

/* Returns true while input should stay blocked due to turn animation. */
bool turn_anim_active(const TurnAnim *anim) {
    return anim->active;
}

/* Builds a new turn animation from pre/post move board states and path. */
void turn_anim_start(
    TurnAnim *anim,
    const uint8_t *before,
    const uint8_t *final_board,
    int from_idx,
    int to_idx,
    const int *path,
    int path_len
) {
    turn_anim_init(anim);

    anim->active = true;
    anim->phase = TURN_PHASE_MOVE;
    anim->phase_t = 0.0f;
    anim->move_dur = 0.18f;
    anim->clear_dur = 0.16f;
    anim->spawn_dur = 0.18f;

    anim->move.active = path_len >= 2;
    anim->move.path_len = clamp_path_len(path_len);
    anim->move.color = idx_ok(from_idx) ? before[from_idx] : 0;
    if (anim->move.active && anim->move.path_len > 0) {
        memcpy(anim->move.path, path, (size_t)anim->move.path_len * sizeof(int));
    }

    copy_board(anim->before_board, before);
    copy_board(anim->final_board, final_board);
    copy_board(anim->after_move_board, before);

    if (idx_ok(from_idx) && idx_ok(to_idx)) {
        anim->after_move_board[to_idx] = anim->after_move_board[from_idx];
        anim->after_move_board[from_idx] = 0;
    }

    copy_board(anim->after_clear_board, anim->after_move_board);

    bool moved_survived = idx_ok(to_idx) && anim->move.color != 0 && final_board[to_idx] == anim->move.color;

    for (int idx = 0; idx < GAME_CELLS; ++idx) {
        if (before[idx] != 0 && final_board[idx] == 0 && idx != from_idx) {
            anim->cleared_idx[anim->cleared_count] = idx;
            anim->cleared_color[anim->cleared_count] = before[idx];
            ++anim->cleared_count;
            anim->after_clear_board[idx] = 0;
        } else if (before[idx] == 0 && final_board[idx] != 0) {
            if (idx == to_idx && moved_survived) {
                continue;
            }
            anim->spawned_idx[anim->spawned_count] = idx;
            anim->spawned_color[anim->spawned_count] = final_board[idx];
            ++anim->spawned_count;
        }
    }

    if (idx_ok(to_idx) && anim->move.color != 0 && !moved_survived) {
        bool present = false;
        for (int i = 0; i < anim->cleared_count; ++i) {
            if (anim->cleared_idx[i] == to_idx) {
                present = true;
                break;
            }
        }
        if (!present) {
            anim->cleared_idx[anim->cleared_count] = to_idx;
            anim->cleared_color[anim->cleared_count] = anim->move.color;
            ++anim->cleared_count;
            anim->after_clear_board[to_idx] = 0;
        }
    }
}

/* Fills render board for the beginning of turn animation. */
void turn_anim_begin_render(const TurnAnim *anim, uint8_t *render_board, size_t cells) {
    if (cells < GAME_CELLS) {
        return;
    }

    copy_board(render_board, anim->before_board);
    if (anim->move.active && anim->move.path_len >= 1) {
        int from_idx = anim->move.path[0];
        if (idx_ok(from_idx)) {
            render_board[from_idx] = 0;
        }
    }
}

/* Advances animation by dt and updates render board snapshot. */
void turn_anim_update(
    TurnAnim *anim,
    float dt,
    uint8_t *render_board,
    size_t cells,
    bool *emit_clear_particles
) {
    if (emit_clear_particles != NULL) {
        *emit_clear_particles = false;
    }
    if (!anim->active || cells < GAME_CELLS) {
        return;
    }

    anim->phase_t += dt;

    if (anim->phase == TURN_PHASE_MOVE) {
        if (anim->phase_t >= anim->move_dur) {
            anim->phase = TURN_PHASE_CLEAR;
            anim->phase_t = 0.0f;
            copy_board(render_board, anim->after_move_board);
            if (emit_clear_particles != NULL) {
                *emit_clear_particles = true;
            }
            copy_board(render_board, anim->after_clear_board);
            if (anim->cleared_count == 0) {
                anim->phase = TURN_PHASE_SPAWN;
                anim->phase_t = 0.0f;
            }
        }
        return;
    }

    if (anim->phase == TURN_PHASE_CLEAR) {
        copy_board(render_board, anim->after_clear_board);
        if (anim->phase_t >= anim->clear_dur) {
            anim->phase = TURN_PHASE_SPAWN;
            anim->phase_t = 0.0f;
        }
        return;
    }

    if (anim->phase == TURN_PHASE_SPAWN) {
        copy_board(render_board, anim->after_clear_board);
        for (int i = 0; i < anim->spawned_count; ++i) {
            int idx = anim->spawned_idx[i];
            if (idx_ok(idx)) {
                render_board[idx] = anim->spawned_color[i];
            }
        }

        if (anim->phase_t >= anim->spawn_dur) {
            copy_board(render_board, anim->final_board);
            turn_anim_init(anim);
        }
    }
}

/* Returns spawn scale [0..1] for newly spawned ball in spawn phase, or -1 otherwise. */
float turn_anim_spawn_scale_for_index(const TurnAnim *anim, int idx) {
    if (!anim->active || anim->phase != TURN_PHASE_SPAWN || anim->spawn_dur <= 0.0f) {
        return -1.0f;
    }

    bool found = false;
    for (int i = 0; i < anim->spawned_count; ++i) {
        if (anim->spawned_idx[i] == idx) {
            found = true;
            break;
        }
    }
    if (!found) {
        return -1.0f;
    }

    float k = anim->phase_t / anim->spawn_dur;
    if (k < 0.0f) {
        k = 0.0f;
    }
    if (k > 1.0f) {
        k = 1.0f;
    }
    return k;
}

/* Returns interpolated move coordinate u in [0, path_len-1] during move phase. */
bool turn_anim_move_u(const TurnAnim *anim, float *u) {
    if (!anim->active || anim->phase != TURN_PHASE_MOVE || !anim->move.active || anim->move.path_len < 2 || anim->move_dur <= 0.0f) {
        return false;
    }

    float v = (anim->phase_t / anim->move_dur) * (float)(anim->move.path_len - 1);
    if (v < 0.0f) {
        v = 0.0f;
    }
    if (v > (float)(anim->move.path_len - 1)) {
        v = (float)(anim->move.path_len - 1);
    }

    if (u != NULL) {
        *u = v;
    }
    return true;
}

/* Returns number of cells cleared in clear phase. */
int turn_anim_cleared_count(const TurnAnim *anim) {
    return anim->cleared_count;
}

/* Returns cleared cell board index for i-th cleared ball. */
int turn_anim_cleared_idx(const TurnAnim *anim, int i) {
    if (i < 0 || i >= anim->cleared_count) {
        return -1;
    }
    return anim->cleared_idx[i];
}

/* Returns cleared ball color for i-th cleared ball. */
uint8_t turn_anim_cleared_color(const TurnAnim *anim, int i) {
    if (i < 0 || i >= anim->cleared_count) {
        return 0;
    }
    return anim->cleared_color[i];
}
