#include "audio.h"

// update interrupt status
static void update_int_sts(audio_state* state) {
    uint32_t status = 0;

    if (state->RIRBSTS & (1 << 0)) {
        status |= (1 << 30);
    }
    if (state->RIRBSTS & (1 << 2)) {
        status |= (1 << 30);
    }
    if (state->STATESTS & state->WAKEEN) {
        status |= (1 << 30);
    }

    for (int i = 0; i < 8; i++) {
        if (state->streams[i].CTL & (1 << 26)) {
            status |= (1 << i);
        }
    }

    if (status & state->INTCTL) {
        status |= (1U << 31);
    }

    state->INTSTS = status;
}

static int send_command(audio_state* state, uint32_t verb) {
    uint32_t codec_addr; nid; data;
    return 0;
}