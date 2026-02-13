#include <stdio.h>
#include <string.h>

#include "audio_fx.h"

/* Ensures audio API is safe when device is not initialized. */
static int test_no_device_calls_are_safe(void) {
    AudioFx fx;
    memset(&fx, 0, sizeof(fx));

    audio_fx_play_tone(&fx, 440.0f, 20, 0.1f);
    audio_fx_play_select(&fx);
    audio_fx_play_invalid(&fx);
    audio_fx_play_move(&fx);
    audio_fx_play_line_clear(&fx, 9);
    audio_fx_play_restart(&fx);
    audio_fx_play_game_over(&fx);
    audio_fx_shutdown(&fx);
    return 0;
}

int main(void) {
    if (test_no_device_calls_are_safe() != 0) {
        return 1;
    }

    printf("Audio fx safety tests passed.\n");
    return 0;
}
