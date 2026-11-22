/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GameCube controllers as Classic Controllers' decisions (rtgcwii.h). */
/* The Wii controller stand-ins' own file: linked only into the pad builds
 * with them (RT_PAD_STANDIN, Makefile.runtime), and compiled, unused, with
 * the DOL's and the host library's runtime sources; it takes rthome.h's
 * stand-in declarations in each. */
#ifndef RT_PAD_STANDIN
#define RT_PAD_STANDIN
#endif
#include "rtgcwii.h"

#include "rthome.h"

#define RTGCWII_PAD_ERR 10u

/* Ticks since a time noted with its low bit set (so 0 can mean "none"). */
static uint32_t rtgcwii_elapsed(uint32_t now, uint32_t noted) { return now - (noted & ~1u); }

static uint32_t rtgcwii_pad_buttons(const uint8_t* s) { return ((uint32_t)s[0] << 8) | s[1]; }

void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now) {
    uint32_t i, k;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint8_t* s = status + i * RTGCWII_PAD_BYTES;
        const uint32_t bit = 1u << i;
        if ((int8_t)s[RTGCWII_PAD_ERR] == 0) {
            for (k = 0; k < RTGCWII_PAD_BYTES; ++k) w->pad[i][k] = s[k];
            w->seen[i] = now | 1u;
            w->hold[i] = rthome_map_classic(rtgcwii_pad_buttons(s), RTHOME_CL_PLUS);
            /* Newly plugged in: what it holds already isn't newly pressed. */
            if (!(w->present & bit)) w->delivered[i] = w->hold[i];
            ++w->samples[i];
            w->present |= bit;
        } else if ((w->present & bit) && rtgcwii_elapsed(now, w->seen[i]) >= w->grace) {
            w->present &= ~bit;
            w->seen[i] = 0;
            w->hold[i] = w->delivered[i] = 0;
        } else if (w->present & bit) {
            /* A dropped report, within the grace period: the buttons stay
             * as held, but it is a new sample, so a press already given to
             * the game isn't given again at each read until a report comes. */
            ++w->samples[i];
        }
    }
}

void rtgcwii_probed(struct rtgcwii* w, uint32_t channel, int32_t result) {
    uint32_t bit;
    if (channel >= RTGCWII_CHANNELS || result == -2) return;  /* busy: no answer either way */
    bit = 1u << channel;
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

int rtgcwii_should_read(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms) {
    if (w->reading) return 0;
    if (w->game_read != 0) {
        if (rtgcwii_elapsed(now, w->game_read) <= 50u * ticks_per_ms) return 0;
        w->game_read = 0;
    }
    if (w->own_read != 0 && rtgcwii_elapsed(now, w->own_read) < 8u * ticks_per_ms) return 0;
    w->own_read = now | 1u;
    return 1;
}

void rtgcwii_read_by(struct rtgcwii* w, uint32_t now, int own) {
    if (!own) w->game_read = now | 1u;
}

void rtgcwii_classic(struct rtgcwii* w, uint32_t channel, uint32_t* hold, uint32_t* trig, uint32_t* release,
                     int32_t axes[6]) {
    uint32_t i;
    if (!rtgcwii_on(w, channel)) {
        *hold = *trig = *release = 0;
        for (i = 0; i < 6; ++i) axes[i] = 0;
        return;
    }
    if (w->worked[channel] != w->samples[channel]) {
        w->worked[channel] = w->samples[channel];
        w->trig[channel] = w->hold[channel] & ~w->delivered[channel];
        w->release[channel] = w->delivered[channel] & ~w->hold[channel];
        w->delivered[channel] = w->hold[channel];
    }
    *hold = w->hold[channel];
    *trig = w->trig[channel];
    *release = w->release[channel];
    rthome_classic_axes(w->pad[channel], axes);
}

uint32_t rtgcwii_hold(const struct rtgcwii* w, uint32_t channel) {
    return rtgcwii_on(w, channel) ? w->hold[channel] : 0u;
}

int rtgcwii_next_change(struct rtgcwii* w, uint32_t skip, uint32_t keep, uint32_t* channel, int32_t* reason) {
    uint32_t leave = 0, join = 0, i;
    const uint32_t now_on = w->present & ~w->real;
    join = now_on & ~w->told & ~skip;
    leave = w->told & ~now_on & ~w->real & ~skip & ~keep;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint32_t bit = 1u << i;
        if (!((join | leave) & bit)) continue;
        *channel = i;
        *reason = (join & bit) ? 0 : -1;
        if (join & bit) {
            w->told |= bit;
        } else {
            w->told &= ~bit;
        }
        return 1;
    }
    return 0;
}

void rtgcwii_kpad_read(struct rtgcwii* w, uint32_t channel, int32_t result, int32_t error) {
    if (channel >= RTGCWII_CHANNELS) return;
    if (result > 0) {
        rtgcwii_probed(w, channel, 0);
    } else if (error == -1) {
        rtgcwii_probed(w, channel, -1);
    }
}

void rtgcwii_still_told(struct rtgcwii* w, uint32_t channel) {
    if (channel < RTGCWII_CHANNELS) w->told |= 1u << channel;
}

void rtgcwii_not_told(struct rtgcwii* w, uint32_t channel) {
    if (channel < RTGCWII_CHANNELS) w->told &= ~(1u << channel);
}
