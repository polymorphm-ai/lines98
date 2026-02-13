#ifndef TURN_ANIM_H
#define TURN_ANIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "game.h"

#define TA_MAX_PATH_NODES GAME_CELLS

/* Per-move interpolation data for rendering the moving ball. */
typedef struct {
    bool active;
    int path[TA_MAX_PATH_NODES];
    int path_len;
    uint8_t color;
} MoveAnim;

/* Ordered phases of one full turn animation. */
typedef enum {
    TURN_PHASE_NONE = 0,
    TURN_PHASE_MOVE = 1,
    TURN_PHASE_CLEAR = 2,
    TURN_PHASE_SPAWN = 3
} TurnPhase;

/* Full turn animation state and board snapshots for phase-by-phase rendering. */
typedef struct {
    bool active;
    TurnPhase phase;
    float phase_t;
    float move_dur;
    float clear_dur;
    float spawn_dur;

    MoveAnim move;

    int cleared_idx[GAME_CELLS];
    uint8_t cleared_color[GAME_CELLS];
    int cleared_count;

    int spawned_idx[GAME_CELLS];
    uint8_t spawned_color[GAME_CELLS];
    int spawned_count;

    uint8_t before_board[GAME_CELLS];
    uint8_t after_move_board[GAME_CELLS];
    uint8_t after_clear_board[GAME_CELLS];
    uint8_t final_board[GAME_CELLS];
} TurnAnim;

/* Resets animation state to idle. */
void turn_anim_init(TurnAnim *anim);

/* Returns true while input should stay blocked due to turn animation. */
bool turn_anim_active(const TurnAnim *anim);

/* Builds a new turn animation from pre/post move board states and path. */
void turn_anim_start(
    TurnAnim *anim,
    const uint8_t *before,
    const uint8_t *final_board,
    int from_idx,
    int to_idx,
    const int *path,
    int path_len
);

/* Fills render board for the beginning of turn animation. */
void turn_anim_begin_render(const TurnAnim *anim, uint8_t *render_board, size_t cells);

/* Advances animation by dt and updates render board snapshot. */
void turn_anim_update(
    TurnAnim *anim,
    float dt,
    uint8_t *render_board,
    size_t cells,
    bool *emit_clear_particles
);

/* Returns spawn scale [0..1] for newly spawned ball in spawn phase, or -1 otherwise. */
float turn_anim_spawn_scale_for_index(const TurnAnim *anim, int idx);

/* Returns interpolated move coordinate u in [0, path_len-1] during move phase. */
bool turn_anim_move_u(const TurnAnim *anim, float *u);

/* Returns number of cells cleared in clear phase. */
int turn_anim_cleared_count(const TurnAnim *anim);

/* Returns cleared cell board index for i-th cleared ball. */
int turn_anim_cleared_idx(const TurnAnim *anim, int i);

/* Returns cleared ball color for i-th cleared ball. */
uint8_t turn_anim_cleared_color(const TurnAnim *anim, int i);

#endif
