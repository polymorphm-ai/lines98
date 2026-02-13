/* SDL audio helper module for procedural UI/gameplay tones. */

#include "audio_fx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Clamps gain/envelope values to avoid clipping. */
static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* Rich note synthesizer with simple harmonic color and vibrato. */
static void queue_note(AudioFx *fx, float frequency, int duration_ms, float gain, float vibrato_hz, float vibrato_depth) {
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
    float base_step = 2.0f * (float)M_PI * frequency / (float)fx->spec.freq;
    float vib_phase = 0.0f;
    float vib_step = 2.0f * (float)M_PI * vibrato_hz / (float)fx->spec.freq;

    for (int i = 0; i < sample_count; ++i) {
        float t = (float)i / (float)sample_count;
        float attack = SDL_min(1.0f, t * 28.0f);
        float release = SDL_min(1.0f, (1.0f - t) * 12.0f);
        float env = attack * release;
        float vib = sinf(vib_phase) * vibrato_depth;
        float carrier = sinf(phase + vib);
        float harmonic2 = 0.35f * sinf(2.0f * phase + vib * 0.5f);
        float harmonic3 = 0.12f * sinf(3.0f * phase + vib * 0.25f);
        samples[i] = (carrier + harmonic2 + harmonic3) * gain * env;
        phase += base_step;
        vib_phase += vib_step;
    }

    SDL_QueueAudio(fx->device, samples, (Uint32)(sample_count * (int)sizeof(float)));
    free(samples);
}

/* Queues a short frequency glide for musical transitions. */
static void queue_glide(AudioFx *fx, float f0, float f1, int duration_ms, float gain, float vibrato_hz, float vibrato_depth) {
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
    float vib_phase = 0.0f;
    float vib_step = 2.0f * (float)M_PI * vibrato_hz / (float)fx->spec.freq;
    for (int i = 0; i < sample_count; ++i) {
        float t = (float)i / (float)sample_count;
        float f = f0 + (f1 - f0) * t;
        float step = 2.0f * (float)M_PI * f / (float)fx->spec.freq;
        float attack = SDL_min(1.0f, t * 24.0f);
        float release = SDL_min(1.0f, (1.0f - t) * 8.0f);
        float env = attack * release;
        float vib = sinf(vib_phase) * vibrato_depth;
        float s0 = sinf(phase + vib);
        float s1 = 0.30f * sinf(2.0f * phase + vib * 0.5f);
        float s2 = 0.08f * sinf(4.0f * phase + vib * 0.2f);
        samples[i] = (s0 + s1 + s2) * clampf(gain, 0.0f, 0.35f) * env;
        phase += step;
        vib_phase += vib_step;
    }

    SDL_QueueAudio(fx->device, samples, (Uint32)(sample_count * (int)sizeof(float)));
    free(samples);
}

/* Queues a subtle broadband burst for particle-like texture. */
static void queue_noise_burst(AudioFx *fx, int duration_ms, float gain) {
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

    float state = 0.0f;
    for (int i = 0; i < sample_count; ++i) {
        float t = (float)i / (float)sample_count;
        float attack = SDL_min(1.0f, t * 40.0f);
        float release = SDL_min(1.0f, (1.0f - t) * 10.0f);
        float env = attack * release;
        float white = (float)rand() / (float)RAND_MAX;
        white = white * 2.0f - 1.0f;
        state = state * 0.78f + white * 0.22f;
        samples[i] = state * clampf(gain, 0.0f, 0.20f) * env;
    }

    SDL_QueueAudio(fx->device, samples, (Uint32)(sample_count * (int)sizeof(float)));
    free(samples);
}

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
    queue_note(fx, frequency, duration_ms, gain, 4.5f, 0.05f);
}

/* Plays short selection cue. */
void audio_fx_play_select(AudioFx *fx) {
    queue_note(fx, 560.0f, 45, 0.07f, 7.0f, 0.04f);
    queue_note(fx, 700.0f, 55, 0.09f, 7.0f, 0.04f);
    queue_note(fx, 840.0f, 65, 0.10f, 7.0f, 0.04f);
}

/* Plays short invalid-action cue. */
void audio_fx_play_invalid(AudioFx *fx) {
    queue_note(fx, 246.0f, 52, 0.10f, 2.5f, 0.02f);
    queue_glide(fx, 246.0f, 164.0f, 110, 0.11f, 2.5f, 0.02f);
}

/* Plays move-start cue. */
void audio_fx_play_move(AudioFx *fx) {
    queue_note(fx, 320.0f, 35, 0.06f, 5.0f, 0.03f);
    queue_glide(fx, 380.0f, 520.0f, 72, 0.09f, 5.5f, 0.03f);
    queue_note(fx, 620.0f, 48, 0.08f, 6.0f, 0.04f);
}

/* Plays line-clear cue; intensity depends on number of cleared balls. */
void audio_fx_play_line_clear(AudioFx *fx, int cleared_count) {
    if (cleared_count < 5) {
        cleared_count = 5;
    }
    if (cleared_count > 20) {
        cleared_count = 20;
    }

    float bonus = (float)(cleared_count - 5) * 12.0f;
    float root = 680.0f + bonus;
    float gain = 0.14f + (float)(cleared_count - 5) * 0.007f;
    if (gain > 0.24f) {
        gain = 0.24f;
    }

    queue_noise_burst(fx, 70, 0.08f + (float)(cleared_count - 5) * 0.004f);
    queue_note(fx, root, 70, gain * 0.9f, 6.0f, 0.04f);
    queue_note(fx, root * 1.25f, 70, gain, 6.5f, 0.05f);
    queue_note(fx, root * 1.5f, 94, gain * 0.95f, 7.0f, 0.05f);
    queue_glide(fx, root * 1.8f, root * 2.2f, 120, gain * 0.82f, 8.0f, 0.07f);
}

/* Plays restart cue. */
void audio_fx_play_restart(AudioFx *fx) {
    queue_note(fx, 392.0f, 75, 0.10f, 5.0f, 0.03f);
    queue_note(fx, 523.3f, 85, 0.12f, 5.5f, 0.03f);
    queue_glide(fx, 622.0f, 698.5f, 110, 0.13f, 6.0f, 0.04f);
}

/* Plays longer game-over composition. */
void audio_fx_play_game_over(AudioFx *fx) {
    if (!fx->ready) {
        return;
    }

    SDL_ClearQueuedAudio(fx->device);
    queue_note(fx, 392.0f, 170, 0.10f, 4.0f, 0.03f);
    queue_note(fx, 349.2f, 190, 0.11f, 4.0f, 0.03f);
    queue_note(fx, 311.1f, 210, 0.12f, 4.2f, 0.04f);
    queue_glide(fx, 293.7f, 246.9f, 280, 0.13f, 4.2f, 0.04f);
    queue_note(fx, 261.6f, 290, 0.14f, 4.5f, 0.05f);
    queue_note(fx, 220.0f, 330, 0.15f, 4.6f, 0.06f);
    queue_note(fx, 196.0f, 370, 0.15f, 4.8f, 0.07f);
    queue_glide(fx, 174.6f, 130.8f, 560, 0.13f, 5.0f, 0.08f);
    queue_noise_burst(fx, 180, 0.04f);
}
