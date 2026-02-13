#ifndef AUDIO_FX_H
#define AUDIO_FX_H

#include <stdbool.h>

#include <SDL2/SDL.h>

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

#endif
