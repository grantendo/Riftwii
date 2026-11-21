/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GameCube controllers as Classic Controllers' decisions (rtgcwii.h). */
#include "rtgcwii.h"

#include "rthome.h"

#define RTGCWII_PAD_ERR 10u

/* Ticks since a time noted with its low bit set (so 0 can mean "none"). */
static uint32_t elapsed(uint32_t now, uint32_t noted) { return now - (noted & ~1u); }

static uint32_t pad_buttons(const uint8_t* s) { return ((uint32_t)s[0] << 8) | s[1]; }

void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now) {
    uint32_t i, k;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint8_t* s = status + i * RTGCWII_PAD_BYTES;
        const uint32_t bit = 1u << i;
        if ((int8_t)s[RTGCWII_PAD_ERR] == 0) {
            for (k = 0; k < RTGCWII_PAD_BYTES; ++k) w->pad[i][k] = s[k];
            w->seen[i] = now | 1u;
            /* Newly plugged in: nothing counts as newly pressed against
             * whatever the channel held before it left. */
            w->last[i] = (w->present & bit) ? w->hold[i] : rthome_map_classic(pad_buttons(s), RTHOME_CL_PLUS);
            w->hold[i] = rthome_map_classic(pad_buttons(s), RTHOME_CL_PLUS);
            w->present |= bit;
        } else if ((w->present & bit) && elapsed(now, w->seen[i]) >= w->grace) {
            w->present &= ~bit;
            w->seen[i] = 0;
            w->last[i] = w->hold[i] = 0;
        }
    }
}

void rtgcwii_probed(struct rtgcwii* w, uint32_t channel, int32_t result) {
    const uint32_t bit = 1u << channel;
    if (channel >= RTGCWII_CHANNELS || result == -2) return;  /* busy: no answer either way */
    if (result == 0) {
        w->real |= bit;
    } else if (w->real & bit) {
        /* The real controller left, and the game's library told the game
         * so: the stand-in is announced again. */
        w->real &= ~bit;
        w->told &= ~bit;
    }
}

int rtgcwii_on(const struct rtgcwii* w, uint32_t channel) {
    if (channel >= RTGCWII_CHANNELS) return 0;
    return ((w->present & ~w->real) >> channel) & 1u;
}

void rtgcwii_connect_change(struct rtgcwii* w, uint32_t* leave, uint32_t* join) {
    const uint32_t now_on = w->present & ~w->real;
    *join = now_on & ~w->told;
    /* Only channels it was told about and no real controller holds. */
    *leave = w->told & ~now_on & ~w->real;
    w->told = (w->told | *join) & ~*leave;
}

void rtgcwii_classic(const struct rtgcwii* w, uint32_t channel, uint32_t* hold, uint32_t* trig, uint32_t* release,
                     int32_t axes[6]) {
    uint32_t i;
    if (!rtgcwii_on(w, channel)) {
        *hold = *trig = *release = 0;
        for (i = 0; i < 6; ++i) axes[i] = 0;
        return;
    }
    *hold = w->hold[channel];
    *trig = w->hold[channel] & ~w->last[channel];
    *release = w->last[channel] & ~w->hold[channel];
    rthome_classic_axes(w->pad[channel], axes);
}
