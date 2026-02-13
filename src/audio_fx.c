/* SDL audio helper module for procedural UI/gameplay tones. */

#include "audio_fx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Initializes SDL audio device for procedural tone playback. */
void audio_fx_init(AudioFx *fx) {
    memset(fx, 0, sizeof(*fx));

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;

    fx->device = SDL_OpenAudioDevice(NULL, 0, &want, &fx->spec, 0);
    if (fx->device != 0) {
        SDL_PauseAudioDevice(fx->device, 0);
        fx->ready = true;
    }
}

/* Releases SDL audio resources owned by AudioFx. */
void audio_fx_shutdown(AudioFx *fx) {
    if (fx->device != 0) {
        SDL_ClearQueuedAudio(fx->device);
        SDL_CloseAudioDevice(fx->device);
        fx->device = 0;
    }
    fx->ready = false;
}

/* Plays one short procedural tone if audio device is available. */
void audio_fx_play_tone(AudioFx *fx, float frequency, int duration_ms, float gain) {
    if (!fx->ready || duration_ms <= 0) {
        return;
    }

    int sample_count = (fx->spec.freq * duration_ms) / 1000;
    if (sample_count <= 0) {
        return;
    }

    float *samples = (float *)malloc((size_t)sample_count * sizeof(float));
    if (samples == NULL) {
        return;
    }

    float phase = 0.0f;
    float step = 2.0f * (float)M_PI * frequency / (float)fx->spec.freq;
    for (int i = 0; i < sample_count; ++i) {
        float env = 1.0f - (float)i / (float)sample_count;
        samples[i] = sinf(phase) * gain * env;
        phase += step;
    }

    SDL_QueueAudio(fx->device, samples, (Uint32)(sample_count * (int)sizeof(float)));
    free(samples);
}
