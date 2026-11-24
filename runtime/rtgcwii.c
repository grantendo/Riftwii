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

/* PADStatus: buttons (big-endian) at 0, the C-stick at 4 and 5, the
 * analog L and R at 6 and 7. GameCube buttons as rthome.c has them. */
#define RTGCWII_GC_LEFT 0x0001u
#define RTGCWII_GC_RIGHT 0x0002u
#define RTGCWII_GC_DOWN 0x0004u
#define RTGCWII_GC_UP 0x0008u
#define RTGCWII_GC_Z 0x0010u
#define RTGCWII_GC_R 0x0020u
#define RTGCWII_GC_L 0x0040u
#define RTGCWII_GC_A 0x0100u
#define RTGCWII_GC_B 0x0200u
#define RTGCWII_GC_X 0x0400u
#define RTGCWII_GC_Y 0x0800u
#define RTGCWII_GC_START 0x1000u
/* A Wii Remote + Nunchuk's buttons in KPADStatus.hold. */
#define RTGCWII_WR_LEFT 0x0001u
#define RTGCWII_WR_RIGHT 0x0002u
#define RTGCWII_WR_DOWN 0x0004u
#define RTGCWII_WR_UP 0x0008u
#define RTGCWII_WR_PLUS 0x0010u
#define RTGCWII_WR_2 0x0100u
#define RTGCWII_WR_1 0x0200u
#define RTGCWII_WR_B 0x0400u
#define RTGCWII_WR_A 0x0800u
#define RTGCWII_WR_MINUS 0x1000u
#define RTGCWII_NC_Z 0x2000u
#define RTGCWII_NC_C 0x4000u
#define RTGCWII_TRIGGER_ON 85u  /* of 255: an analog L or R past a third of the way */
#define RTGCWII_STICK_PRESS 128  /* of RTHOME_AXIS_FULL: the control stick past half way is the D-pad */

uint32_t rtgcwii_map_nunchuk(const uint8_t* pad) {
    uint32_t b = rtgcwii_pad_buttons(pad), h = 0;
    if (pad[6] >= RTGCWII_TRIGGER_ON) b |= RTGCWII_GC_L;
    if (pad[7] >= RTGCWII_TRIGGER_ON) b |= RTGCWII_GC_R;
    if (b & RTGCWII_GC_A) h |= RTGCWII_WR_A;
    if (b & RTGCWII_GC_B) h |= RTGCWII_WR_B;
    if (b & RTGCWII_GC_Y) h |= RTGCWII_WR_2;
    if (b & RTGCWII_GC_Z) h |= RTGCWII_WR_MINUS;
    if (b & RTGCWII_GC_START) h |= RTGCWII_WR_PLUS;
    if (b & RTGCWII_GC_L) h |= RTGCWII_NC_Z;
    if (b & RTGCWII_GC_R) h |= RTGCWII_NC_C;
    if (b & RTGCWII_GC_LEFT) h |= RTGCWII_WR_LEFT;
    if (b & RTGCWII_GC_RIGHT) h |= RTGCWII_WR_RIGHT;
    if (b & RTGCWII_GC_UP) h |= RTGCWII_WR_UP;
    if (b & RTGCWII_GC_DOWN) h |= RTGCWII_WR_DOWN;
    return h;
}

uint32_t rtgcwii_map_sideways(const uint8_t* pad) {
    uint32_t b = rtgcwii_pad_buttons(pad), h = 0;
    int32_t axes[6];
    rthome_classic_axes(pad, axes);
    if (axes[0] <= -RTGCWII_STICK_PRESS) b |= RTGCWII_GC_LEFT;
    if (axes[0] >= RTGCWII_STICK_PRESS) b |= RTGCWII_GC_RIGHT;
    if (axes[1] >= RTGCWII_STICK_PRESS) b |= RTGCWII_GC_UP;
    if (axes[1] <= -RTGCWII_STICK_PRESS) b |= RTGCWII_GC_DOWN;
    if (b & RTGCWII_GC_A) h |= RTGCWII_WR_2;
    if (b & RTGCWII_GC_B) h |= RTGCWII_WR_1;
    if (b & RTGCWII_GC_X) h |= RTGCWII_WR_A;
    if (b & RTGCWII_GC_Y) h |= RTGCWII_WR_B;
    if (b & RTGCWII_GC_Z) h |= RTGCWII_WR_MINUS;
    if (b & RTGCWII_GC_START) h |= RTGCWII_WR_PLUS;
    /* Held sideways, D-pad on the left: the remote's Up points left. */
    if (b & RTGCWII_GC_LEFT) h |= RTGCWII_WR_UP;
    if (b & RTGCWII_GC_RIGHT) h |= RTGCWII_WR_DOWN;
    if (b & RTGCWII_GC_UP) h |= RTGCWII_WR_RIGHT;
    if (b & RTGCWII_GC_DOWN) h |= RTGCWII_WR_LEFT;
    return h;
}

#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
/* Once a sample: the pointer follows the C-stick (in the sideways form
 * only while the game has the camera on: else the C-stick tilts), and the
 * shake counts while its button is held (X; R in the sideways form, also
 * from the analog trigger). */
static void rtgcwii_motion(struct rtgcwii* w, uint32_t i, const uint8_t* s, uint32_t now) {
    int32_t axes[6], k;
    uint32_t shaking;
    rthome_classic_axes(s, axes);  /* the C-stick is axes 2 and 3 */
#ifdef RT_PAD_FORM_SIDEWAYS
    shaking = (rtgcwii_pad_buttons(s) & RTGCWII_GC_R) || s[7] >= RTGCWII_TRIGGER_ON;
    w->shake[i] = shaking ? w->shake[i] + 1u : 0u;
    if (!((w->camera >> i) & 1u)) return;
#else
    shaking = rtgcwii_pad_buttons(s) & RTGCWII_GC_X;
#endif
    for (k = 0; k < 2; ++k) {
        /* Up on the C-stick is up on the screen (KPAD's y grows downward). */
        const int32_t step = (k == 0 ? axes[2] : -axes[3]) * RTGCWII_POINTER_STEP / RTHOME_AXIS_FULL;
        int32_t p = w->pointer[i][k] + step;
        if (p > RTGCWII_POINTER_EDGE) p = RTGCWII_POINTER_EDGE;
        if (p < -RTGCWII_POINTER_EDGE) p = -RTGCWII_POINTER_EDGE;
        w->pointer[i][k] = p;
    }
    if (axes[2] != 0 || axes[3] != 0) {
        w->pointer_moved[i] = now | 1u;
    } else if (w->pointer_moved[i] != 0 && rtgcwii_elapsed(now, w->pointer_moved[i]) >= w->pointer_show) {
        /* Hidden for good until the C-stick moves again: the time base
         * wraps (about every 70 s), and an old mark would read as recent. */
        w->pointer_moved[i] = 0;
    }
#ifndef RT_PAD_FORM_SIDEWAYS
    w->shake[i] = shaking ? w->shake[i] + 1u : 0u;
#endif
}
#endif

void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now) {
    uint32_t i, k;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint8_t* s = status + i * RTGCWII_PAD_BYTES;
        const uint32_t bit = 1u << i;
        if ((int8_t)s[RTGCWII_PAD_ERR] == 0) {
            for (k = 0; k < RTGCWII_PAD_BYTES; ++k) w->pad[i][k] = s[k];
            w->seen[i] = now | 1u;
#if defined(RT_PAD_FORM_NUNCHUK)
            w->hold[i] = rtgcwii_map_nunchuk(s);
            rtgcwii_motion(w, i, s, now);
#elif defined(RT_PAD_FORM_SIDEWAYS)
            w->hold[i] = rtgcwii_map_sideways(s);
            rtgcwii_motion(w, i, s, now);
#else
            w->hold[i] = rthome_map_classic(rtgcwii_pad_buttons(s), RTHOME_CL_PLUS);
#endif
            /* Newly plugged in: what it holds already isn't newly pressed. */
            if (!(w->present & bit)) w->delivered[i] = w->hold[i];
            ++w->samples[i];
            w->present |= bit;
        } else if ((w->present & bit) && rtgcwii_elapsed(now, w->seen[i]) >= w->grace) {
            w->present &= ~bit;
            w->seen[i] = 0;
            w->hold[i] = w->delivered[i] = 0;
#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
            w->shake[i] = 0;
            /* Unplugged: the pointer's last move is forgotten too, so an
             * old mark never reads as recent after the time base wraps. */
            w->pointer_moved[i] = 0;
#endif
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
        /* A controller there now: any count of empty reads starts over. */
        w->empty_since[channel] = 0;
        w->empty_reads[channel] = 0;
    } else if (w->real & bit) {
        /* The real controller left, and the game's library told the game
         * so: the stand-in is announced again. */
        w->real &= ~bit;
        w->told &= ~bit;
        w->empty_since[channel] = 0;
        w->empty_reads[channel] = 0;
    }
}

int rtgcwii_on(const struct rtgcwii* w, uint32_t channel) {
    if (channel >= RTGCWII_CHANNELS) return 0;
    return ((w->present & ~w->real) >> channel) & 1u;
}

uint32_t rtgcwii_shaped(const struct rtgcwii* w) { return (w->present & ~w->real) | w->told | w->home_shaped; }

void rtgcwii_home_took(struct rtgcwii* w, uint32_t channel) {
    w->home_shaped = 0;
    if (channel < RTGCWII_CHANNELS) w->home_shaped = rtgcwii_shaped(w) & (1u << channel);
}

int rtgcwii_home_keeps_shape(const struct rtgcwii* w, uint32_t channel) {
    return channel < RTGCWII_CHANNELS && ((w->home_shaped >> channel) & 1u);
}

void rtgcwii_connect_change(struct rtgcwii* w, uint32_t* leave, uint32_t* join) {
    const uint32_t now_on = w->present & ~w->real;
    *join = now_on & ~w->told;
    /* Only channels it was told about and no real controller holds. */
    *leave = w->told & ~now_on & ~w->real;
    w->told = (w->told | *join) & ~*leave;
}

void rtgcwii_wii_read(struct rtgcwii* w, uint32_t now) { w->wii_read = now | 1u; }

int rtgcwii_quiet_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms) {
    if (w->wii_read != 0) return rtgcwii_elapsed(now, w->wii_read) >= 3000u * ticks_per_ms;
    if (w->quiet_from == 0) {
        w->quiet_from = now | 1u;
        return 0;
    }
    return rtgcwii_elapsed(now, w->quiet_from) >= 5000u * ticks_per_ms;
}

int rtgcwii_real_check_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms) {
    if (w->real == 0) return 0;
    if (w->real_checked != 0 && rtgcwii_elapsed(now, w->real_checked) < 250u * ticks_per_ms) return 0;
    w->real_checked = now | 1u;
    return 1;
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

/* The words for the game's KPAD read: newly pressed and let go against what
 * it last got, worked out once a sample. */
static void rtgcwii_words(struct rtgcwii* w, uint32_t channel, uint32_t* hold, uint32_t* trig, uint32_t* release) {
    if (w->worked[channel] != w->samples[channel]) {
        w->worked[channel] = w->samples[channel];
        w->trig[channel] = w->hold[channel] & ~w->delivered[channel];
        w->release[channel] = w->delivered[channel] & ~w->hold[channel];
        w->delivered[channel] = w->hold[channel];
    }
    *hold = w->hold[channel];
    *trig = w->trig[channel];
    *release = w->release[channel];
}

void rtgcwii_classic(struct rtgcwii* w, uint32_t channel, uint32_t* hold, uint32_t* trig, uint32_t* release,
                     int32_t axes[6]) {
    uint32_t i;
    if (!rtgcwii_on(w, channel)) {
        *hold = *trig = *release = 0;
        for (i = 0; i < 6; ++i) axes[i] = 0;
        return;
    }
    rtgcwii_words(w, channel, hold, trig, release);
    rthome_classic_axes(w->pad[channel], axes);
}

/* A controller held still: gravity along -Y, as KPAD reports a real one at
 * rest (both the Wii Remote's and the Nunchuk's accelerometers). */
static void rtgcwii_still(int32_t acc[3], int32_t* value, int32_t* speed) {
    acc[0] = 0;
    acc[1] = -RTHOME_AXIS_FULL;
    acc[2] = 0;
    *value = RTHOME_AXIS_FULL;
    *speed = 0;
}

/* The shake (rtgcwii_nunchuk's): all three axes swing together, and a
 * flip is a jump in speed. */
static void rtgcwii_shake(uint32_t shake, int32_t acc[3], int32_t* value, int32_t* speed) {
    const int32_t swing = ((shake / RTGCWII_SHAKE_FLIP) & 1u) ? RTGCWII_SHAKE_G : -RTGCWII_SHAKE_G;
    acc[0] = swing;
    acc[1] = swing;
    acc[2] = -swing;
    *value = RTGCWII_SHAKE_VALUE;
    *speed = shake > 1u && (shake % RTGCWII_SHAKE_FLIP) == 0 ? RTGCWII_SHAKE_VALUE : 0;
}

/* The integer square root of `v` (rounded down). */
static int32_t rtgcwii_isqrt(int32_t v) {
    int32_t r = 0, bit = 1 << 30;
    if (v <= 0) return 0;
    while (bit > v) bit >>= 2;
    while (bit != 0) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

void rtgcwii_nunchuk(struct rtgcwii* w, uint32_t channel, uint32_t now, struct rtgcwii_fs* out) {
    int32_t axes[6];
    uint32_t shake;
    rtgcwii_still(out->acc, &out->acc_value, &out->acc_speed);
    rtgcwii_still(out->fs_acc, &out->fs_acc_value, &out->fs_acc_speed);
    out->stick[0] = out->stick[1] = 0;
    out->pointer[0] = out->pointer[1] = 0;
    out->pointer_shown = 0;
    if (!rtgcwii_on(w, channel)) {
        out->hold = out->trig = out->release = 0;
        return;
    }
    rtgcwii_words(w, channel, &out->hold, &out->trig, &out->release);
    rthome_classic_axes(w->pad[channel], axes);
    out->stick[0] = axes[0];
    out->stick[1] = axes[1];
    out->pointer[0] = w->pointer[channel][0];
    out->pointer[1] = w->pointer[channel][1];
    out->pointer_shown = ((w->camera >> channel) & 1u) ||
                         (w->pointer_moved[channel] != 0 && rtgcwii_elapsed(now, w->pointer_moved[channel]) < w->pointer_show);
    shake = w->shake[channel];
    if (shake != 0) rtgcwii_shake(shake, out->acc, &out->acc_value, &out->acc_speed);
}

void rtgcwii_sideways(struct rtgcwii* w, uint32_t channel, struct rtgcwii_fs* out) {
    int32_t axes[6], s;
    rtgcwii_still(out->acc, &out->acc_value, &out->acc_speed);
    rtgcwii_still(out->fs_acc, &out->fs_acc_value, &out->fs_acc_speed);
    out->stick[0] = out->stick[1] = 0;
    out->pointer[0] = out->pointer[1] = 0;
    out->pointer_shown = 0;
    if (!rtgcwii_on(w, channel)) {
        out->hold = out->trig = out->release = 0;
        return;
    }
    rtgcwii_words(w, channel, &out->hold, &out->trig, &out->release);
    if ((w->camera >> channel) & 1u) {
        out->pointer[0] = w->pointer[channel][0];
        out->pointer[1] = w->pointer[channel][1];
        out->pointer_shown = 1;
    } else {
        /* Tilt: the C-stick held right steers right, gravity turning from
         * -Y towards -Z, its length staying 1 g. */
        rthome_classic_axes(w->pad[channel], axes);
        s = -axes[2] * RTGCWII_TILT_SIN / RTHOME_AXIS_FULL;
        out->acc[2] = s;
        out->acc[1] = -rtgcwii_isqrt(RTHOME_AXIS_FULL * RTHOME_AXIS_FULL - s * s);
    }
    if (w->shake[channel] != 0) rtgcwii_shake(w->shake[channel], out->acc, &out->acc_value, &out->acc_speed);
}

void rtgcwii_camera(struct rtgcwii* w, uint32_t channel, uint32_t mode) {
    if (channel >= RTGCWII_CHANNELS) return;
    if (mode != 0) {
        w->camera |= 1u << channel;
    } else {
        w->camera &= ~(1u << channel);
    }
}

uint32_t rtgcwii_float_bits(int32_t numerator) {
    uint32_t sign = 0, magnitude, top = 0;
    if (numerator == 0) return 0;
    /* The size taken unsigned, so the most negative value has one too. */
    magnitude = (uint32_t)numerator;
    if (numerator < 0) {
        sign = 0x80000000u;
        magnitude = 0u - magnitude;
    }
    while (top < 31u && (magnitude >> (top + 1)) != 0) ++top;  /* its highest bit */
    /* magnitude / 256 = 1.mantissa * 2^(top - 8) */
    return sign | ((127u + top - 8u) << 23) |
           ((top <= 23u ? magnitude << (23u - top) : magnitude >> (top - 23u)) & 0x007FFFFFu);
}

uint32_t rtgcwii_hold(const struct rtgcwii* w, uint32_t channel) {
    return rtgcwii_on(w, channel) ? w->hold[channel] : 0u;
}

int rtgcwii_next_change(struct rtgcwii* w, uint32_t skip, uint32_t keep, uint32_t* channel, int32_t* reason) {
    uint32_t leave = 0, join = 0, i;
    const uint32_t now_on = w->present & ~w->real;
    /* A channel due to be told again is told "connected" once more; one
     * that left before that is told it's gone (it still counts as told). */
    join = now_on & (~w->told | w->retell) & ~skip;
    leave = w->told & ~now_on & ~w->real & ~skip & ~keep;
    w->retell &= now_on;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint32_t bit = 1u << i;
        if (!((join | leave) & bit)) continue;
        *channel = i;
        *reason = (join & bit) ? 0 : -1;
        if (join & bit) {
            w->told |= bit;
            w->retell &= ~bit;
            /* A new "connected": only a read after it says the game heard
             * it (a WPADRead before the callback was told doesn't). */
            w->heard &= ~bit;
        } else {
            w->told &= ~bit;
        }
        return 1;
    }
    return 0;
}

void rtgcwii_kpad_read(struct rtgcwii* w, uint32_t channel, int32_t result, int32_t error, uint32_t now) {
    uint32_t bit;
    if (channel >= RTGCWII_CHANNELS) return;
    bit = 1u << channel;
    if (result > 0) {
        w->empty_since[channel] = 0;
        w->empty_reads[channel] = 0;
        rtgcwii_probed(w, channel, 0);
    } else if (error == -1) {
        w->empty_since[channel] = 0;
        w->empty_reads[channel] = 0;
        rtgcwii_probed(w, channel, -1);
    } else if (error == RTGCWII_NO_ERROR && (w->real & bit)) {
        /* A read with no error output can't say "no controller": a real
         * controller that brings no samples for a while (and many reads in
         * a row, so a pause in the game's reading doesn't count) has left. */
        if (w->empty_since[channel] == 0) {
            w->empty_since[channel] = now | 1u;
            w->empty_reads[channel] = 1;
        } else if (++w->empty_reads[channel] >= RTGCWII_EMPTY_READS &&
                   rtgcwii_elapsed(now, w->empty_since[channel]) >= w->grace) {
            w->empty_since[channel] = 0;
            w->empty_reads[channel] = 0;
            rtgcwii_probed(w, channel, -1);
        }
    }
}

void rtgcwii_still_told(struct rtgcwii* w, uint32_t channel) {
    if (channel < RTGCWII_CHANNELS) w->told |= 1u << channel;
}

void rtgcwii_not_told(struct rtgcwii* w, uint32_t channel) {
    if (channel < RTGCWII_CHANNELS) w->told &= ~(1u << channel);
}

void rtgcwii_heard(struct rtgcwii* w, uint32_t channel) {
    if (channel < RTGCWII_CHANNELS) w->heard |= 1u << channel;
}

void rtgcwii_retell(struct rtgcwii* w, uint32_t now) {
    uint32_t i;
    if (w->retell_ticks == 0) return;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint32_t bit = 1u << i;
        if (!rtgcwii_on(w, i)) {
            /* Gone (or a real controller's): the next controller starts afresh. */
            w->heard &= ~bit;
            w->retell &= ~bit;
            w->told_at[i] = 0;
            w->retells[i] = 0;
            continue;
        }
        if (!(w->told & bit) || (w->heard & bit) || (w->retell & bit)) continue;  /* still to tell, heard, or due */
        if (w->told_at[i] == 0) {
            w->told_at[i] = now | 1u;
            continue;
        }
        if (rtgcwii_elapsed(now, w->told_at[i]) < w->retell_ticks || w->retells[i] >= RTGCWII_RETELLS) continue;
        w->retell |= bit;
        w->told_at[i] = 0;
        ++w->retells[i];
    }
}

#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
static void rtgcwii_put16(uint8_t* p, int32_t v) {
    p[0] = (uint8_t)((uint32_t)v >> 8);
    p[1] = (uint8_t)v;
}

/* An acceleration over RTHOME_AXIS_FULL (KPAD's axes: x left, y down along
 * the remote at rest, z its length) as WPAD's three counts (x, y along its
 * length, z up out of its face): KPAD's x, y and z are WPAD's -x, -z and y. */
static void rtgcwii_put_acc(uint8_t* p, const int32_t acc[3]) {
    rtgcwii_put16(p, -acc[0] * RTGCWII_ACC_G / RTHOME_AXIS_FULL);
    rtgcwii_put16(p + 2, acc[2] * RTGCWII_ACC_G / RTHOME_AXIS_FULL);
    rtgcwii_put16(p + 4, -acc[1] * RTGCWII_ACC_G / RTHOME_AXIS_FULL);
}

static void rtgcwii_put_dot(uint8_t* o, int32_t x, int32_t y, uint32_t size, uint32_t id) {
    if (x < 0) x = 0;
    if (x > 1023) x = 1023;
    if (y < 0) y = 0;
    if (y > 767) y = 767;
    rtgcwii_put16(o, x);
    rtgcwii_put16(o + 2, y);
    rtgcwii_put16(o + 4, (int32_t)size);
    o[6] = (uint8_t)id;
    o[7] = 0;
}

/* Data formats with the camera: a bare Wii Remote's and one with a Nunchuk's. */
#define RTGCWII_FMT_CORE_ACC_DPD 2u
#define RTGCWII_FMT_FS_FIRST 3u
#define RTGCWII_FMT_FS_ACC_DPD 5u

uint32_t rtgcwii_wpad_status(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t now, int kpad_camera) {
    struct rtgcwii_fs f;
    uint32_t i, format, bytes = RTGCWII_WPAD_CORE_BYTES;
    int32_t mid_x, mid_y;
    if (channel >= RTGCWII_CHANNELS || !rtgcwii_on(w, channel)) return 0;
    format = w->format[channel];
    rtgcwii_camera(w, channel,
                   kpad_camera || format == RTGCWII_FMT_CORE_ACC_DPD || format == RTGCWII_FMT_FS_ACC_DPD);
    {
        /* The report needs only the buttons held: what the game's KPAD reads
         * were last given (and so what is new at its next one) stays as it
         * was, as for the HOME Menu's stand-in. */
        const uint32_t worked = w->worked[channel], delivered = w->delivered[channel];
        const uint32_t trig = w->trig[channel], release = w->release[channel];
#ifdef RT_PAD_FORM_SIDEWAYS
        (void)now;
        rtgcwii_sideways(w, channel, &f);
#else
        rtgcwii_nunchuk(w, channel, now, &f);
        if (format >= RTGCWII_FMT_FS_FIRST && format <= RTGCWII_FMT_FS_ACC_DPD) bytes = RTGCWII_WPAD_FS_BYTES;
#endif
        w->worked[channel] = worked;
        w->delivered[channel] = delivered;
        w->trig[channel] = trig;
        w->release[channel] = release;
    }
    for (i = 0; i < bytes; ++i) out[i] = 0;
    rtgcwii_put16(out, (int32_t)(f.hold & 0xFFFFu));
    rtgcwii_put_acc(out + 2, f.acc);
    if ((w->camera >> channel) & 1u) {
        if (f.pointer_shown) {
            /* The dots move against the pointer: pointing right or down puts
             * the sensor bar left of or above the camera's centre. */
            mid_x = 512 - f.pointer[0] * RTGCWII_DOT_RANGE_X / RTHOME_POINTER_FULL;
            mid_y = 384 - f.pointer[1] * RTGCWII_DOT_RANGE_Y / RTHOME_POINTER_FULL;
            rtgcwii_put_dot(out + 8, mid_x + RTGCWII_DOT_APART / 2 + 1, mid_y, RTGCWII_DOT_SIZE, 0);
            rtgcwii_put_dot(out + 16, mid_x - RTGCWII_DOT_APART / 2, mid_y, RTGCWII_DOT_SIZE, 1);
        } else {
            rtgcwii_put_dot(out + 8, 0, 767, 0, 0);
            rtgcwii_put_dot(out + 16, 0, 767, 0, 1);
        }
        /* The other two: nothing seen (size 0), as Dolphin's camera gives them. */
        rtgcwii_put_dot(out + 24, 0, 767, 0, 2);
        rtgcwii_put_dot(out + 32, 0, 767, 0, 3);
    }
#ifdef RT_PAD_FORM_NUNCHUK
    out[40] = 1u; /* WPAD_DEV_FREESTYLE */
    if (bytes == RTGCWII_WPAD_FS_BYTES) {
        rtgcwii_put_acc(out + 42, f.fs_acc);
        out[48] = (uint8_t)(int8_t)(f.stick[0] * RTGCWII_FS_STICK_FULL / RTHOME_AXIS_FULL);
        out[49] = (uint8_t)(int8_t)(f.stick[1] * RTGCWII_FS_STICK_FULL / RTHOME_AXIS_FULL);
    }
#endif
    return bytes;
}
#else
uint32_t rtgcwii_wpad_status(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t now, int kpad_camera) {
    (void)w;
    (void)channel;
    (void)out;
    (void)now;
    (void)kpad_camera;
    return 0;
}
#endif
