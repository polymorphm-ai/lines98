#ifndef AUDIO_FX_H
#define AUDIO_FX_H

#include <stdbool.h>

#include <SDL.h>

/* Small owner struct for SDL audio device and obtained format. */
typedef struct {
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;
    bool ready;
} AudioFx;

/* Initializes SDL audio device for procedural tone playback. */
void audio_fx_init(AudioFx *fx);

/* Releases SDL audio resources owned by AudioFx. */
void audio_fx_shutdown(AudioFx *fx);

/* Plays one short procedural tone if audio device is available. */
void audio_fx_play_tone(AudioFx *fx, float frequency, int duration_ms, float gain);

/* Plays short selection cue. */
void audio_fx_play_select(AudioFx *fx);

/* Plays short invalid-action cue. */
void audio_fx_play_invalid(AudioFx *fx);

/* Plays move-start cue. */
void audio_fx_play_move(AudioFx *fx);

/* Plays line-clear cue; intensity depends on number of cleared balls. */
void audio_fx_play_line_clear(AudioFx *fx, int cleared_count);

/* Plays restart cue. */
void audio_fx_play_restart(AudioFx *fx);

/* Plays longer game-over composition. */
void audio_fx_play_game_over(AudioFx *fx);

#endif
