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

/* Ticks since a time noted with its low bit set (so 0 can mean "none"). */
static uint32_t rtgcwii_elapsed(uint32_t now, uint32_t noted) { return now - (noted & ~1u); }

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

static uint32_t rtgcwii_nunchuk_buttons(const struct rtsrc_sample* p) {
    uint32_t b = p->buttons, h = 0;
    if (p->trigger[0] >= RTGCWII_TRIGGER_ON) b |= RTGCWII_GC_L;
    if (p->trigger[1] >= RTGCWII_TRIGGER_ON) b |= RTGCWII_GC_R;
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

uint32_t rtgcwii_map_nunchuk(const uint8_t* pad) {
    struct rtsrc_sample p;
    rtsrc_decode(pad, &p);
    return rtgcwii_nunchuk_buttons(&p);
}

static uint32_t rtgcwii_sideways_buttons(const struct rtsrc_sample* p) {
    uint32_t b = p->buttons, h = 0;
    if (p->axes[0] <= -RTGCWII_STICK_PRESS) b |= RTGCWII_GC_LEFT;
    if (p->axes[0] >= RTGCWII_STICK_PRESS) b |= RTGCWII_GC_RIGHT;
    if (p->axes[1] >= RTGCWII_STICK_PRESS) b |= RTGCWII_GC_UP;
    if (p->axes[1] <= -RTGCWII_STICK_PRESS) b |= RTGCWII_GC_DOWN;
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

uint32_t rtgcwii_map_sideways(const uint8_t* pad) {
    struct rtsrc_sample p;
    rtsrc_decode(pad, &p);
    return rtgcwii_sideways_buttons(&p);
}

#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
/* The shake's count at a sample: by time (every shake_flip ticks a step),
 * else by samples; shake_jump notes a step until a read (rtgcwii_nunchuk,
 * rtgcwii_sideways) takes it, so samples between two reads can't lose it
 * (CodeRabbit CLI, #610). */
static void rtgcwii_count_shake(struct rtgcwii* w, uint32_t i, uint32_t shaking, uint32_t now) {
    const uint32_t bit = 1u << i;
    uint32_t flips;
    if (!shaking) {
        w->shake[i] = 0;
        w->shake_from[i] = 0;
        w->shake_jump &= ~bit;
        return;
    }
    if (w->shake_flip == 0) {
        ++w->shake[i];
        if (w->shake[i] > 1u && (w->shake[i] % RTGCWII_SHAKE_FLIP) == 0) w->shake_jump |= bit;
        return;
    }
    if (w->shake_from[i] == 0) w->shake_from[i] = now | 1u;
    flips = rtgcwii_elapsed(now, w->shake_from[i]) / w->shake_flip;
    /* The count keeps the flips in its RTGCWII_SHAKE_FLIP's (rtgcwii_shake
     * takes the side from it), never 0 while held. */
    {
        const uint32_t count = RTGCWII_SHAKE_FLIP * flips + 1u;
        if (w->shake[i] != 0 && count != w->shake[i]) w->shake_jump |= bit;
        w->shake[i] = count;
    }
}

/* Once a sample: the pointer follows the C-stick (in the sideways form
 * only while the game has the camera on: else the C-stick tilts), and the
 * shake counts while its button is held (X; R in the sideways form, also
 * from the analog trigger). */
static void rtgcwii_motion(struct rtgcwii* w, uint32_t i, const struct rtsrc_sample* s, uint32_t now) {
    const int32_t* axes = s->axes;  /* the C-stick is axes 2 and 3 */
    int32_t k;
    uint32_t shaking;
#ifdef RT_PAD_FORM_SIDEWAYS
    shaking = (s->buttons & RTGCWII_GC_R) || s->trigger[1] >= RTGCWII_TRIGGER_ON;
    rtgcwii_count_shake(w, i, shaking, now);
    if (!((w->camera >> i) & 1u)) return;
#else
    shaking = s->buttons & RTGCWII_GC_X;
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
    rtgcwii_count_shake(w, i, shaking, now);
#endif
}
#endif

void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now) {
    uint32_t i;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint32_t bit = 1u << i;
        const enum rtsrc_event event = rtsrc_port(&w->src, i, status + i * RTGCWII_PAD_BYTES, w->slot.grace, now);
        if (event == RTSRC_ARRIVED || event == RTSRC_REPORT) {
            struct rtsrc_sample p;
            rtsrc_latest(&w->src, i, &p);
#if defined(RT_PAD_FORM_NUNCHUK)
            w->hold[i] = rtgcwii_nunchuk_buttons(&p);
            rtgcwii_motion(w, i, &p, now);
#elif defined(RT_PAD_FORM_SIDEWAYS)
            w->hold[i] = rtgcwii_sideways_buttons(&p);
            rtgcwii_motion(w, i, &p, now);
#else
            w->hold[i] = rthome_map_classic(p.buttons, RTHOME_CL_PLUS);
#endif
            /* Newly plugged in: what it holds already isn't newly pressed. */
            if (event == RTSRC_ARRIVED) w->delivered[i] = w->hold[i];
            w->slot.present |= bit;
        } else if (event == RTSRC_GONE) {
            w->slot.present &= ~bit;
            w->hold[i] = w->delivered[i] = 0;
#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
            w->shake[i] = 0;
            w->shake_from[i] = 0;
            w->shake_jump &= ~bit;
            /* Unplugged: the pointer's last move is forgotten too, so an
             * old mark never reads as recent after the time base wraps. */
            w->pointer_moved[i] = 0;
#endif
        }
    }
}

uint32_t rtgcwii_probe_count(const struct rtgcwii* w, uint32_t channel) { return rtslot_probe_count(&w->slot, channel); }

void rtgcwii_probed(struct rtgcwii* w, uint32_t channel, int32_t result) { rtslot_probed(&w->slot, channel, result); }

int rtgcwii_on(const struct rtgcwii* w, uint32_t channel) { return rtslot_on(&w->slot, channel); }

uint32_t rtgcwii_shaped(const struct rtgcwii* w) { return rtslot_shaped(&w->slot); }

void rtgcwii_home_took(struct rtgcwii* w, uint32_t channel) { rtslot_home_took(&w->slot, channel); }

int rtgcwii_home_keeps_shape(const struct rtgcwii* w, uint32_t channel) { return rtslot_home_keeps_shape(&w->slot, channel); }

void rtgcwii_connect_change(struct rtgcwii* w, uint32_t* leave, uint32_t* join) { rtslot_connect_change(&w->slot, leave, join); }

void rtgcwii_wii_read(struct rtgcwii* w, uint32_t now) { rtslot_wii_read(&w->slot, now); }

int rtgcwii_quiet_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms) { return rtslot_quiet_due(&w->slot, now, ticks_per_ms); }

int rtgcwii_real_check_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms) { return rtslot_real_check_due(&w->slot, now, ticks_per_ms); }

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
    if (w->worked[channel] != w->src.samples[channel]) {
        w->worked[channel] = w->src.samples[channel];
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
    {
        struct rtsrc_sample p;
        rtsrc_latest(&w->src, channel, &p);
        for (i = 0; i < 6; ++i) axes[i] = p.axes[i];
    }
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

/* The shake (rtgcwii_nunchuk's): all three axes swing together, every
 * RTGCWII_SHAKE_STEPS-th step at rest, and a step (`jump`,
 * rtgcwii_count_shake) is a jump in speed. */
static void rtgcwii_shake(uint32_t shake, int jump, int32_t acc[3], int32_t* value, int32_t* speed) {
    const uint32_t step = shake / RTGCWII_SHAKE_FLIP;
    *speed = jump ? RTGCWII_SHAKE_VALUE : 0;
    if (step % RTGCWII_SHAKE_STEPS == RTGCWII_SHAKE_STEPS - 1u) {
        acc[0] = 0;
        acc[1] = -RTHOME_AXIS_FULL;
        acc[2] = 0;
        *value = RTHOME_AXIS_FULL;
        return;
    }
    {
        const int32_t swing = (step & 1u) ? RTGCWII_SHAKE_G : -RTGCWII_SHAKE_G;
        acc[0] = swing;
        acc[1] = swing;
        acc[2] = -swing;
        *value = RTGCWII_SHAKE_VALUE;
    }
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
    struct rtsrc_sample p;
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
    rtsrc_latest(&w->src, channel, &p);
    out->stick[0] = p.axes[0];
    out->stick[1] = p.axes[1];
    out->pointer[0] = w->pointer[channel][0];
    out->pointer[1] = w->pointer[channel][1];
    out->pointer_shown = ((w->camera >> channel) & 1u) ||
                         (w->pointer_moved[channel] != 0 && rtgcwii_elapsed(now, w->pointer_moved[channel]) < w->pointer_show);
    shake = w->shake[channel];
    if (shake != 0) {
        const int jump = (int)((w->shake_jump >> channel) & 1u);
        /* Both hands: Donkey Kong Country Returns' Nunchuk play starts a
         * level only when the Nunchuk shakes too (Dolphin, a real Wii Remote
         * and Nunchuk shaken together). */
        rtgcwii_shake(shake, jump, out->acc, &out->acc_value, &out->acc_speed);
        rtgcwii_shake(shake, jump, out->fs_acc, &out->fs_acc_value, &out->fs_acc_speed);
        w->shake_jump &= ~(1u << channel);  /* this read took the step's jump */
    }
}

void rtgcwii_sideways(struct rtgcwii* w, uint32_t channel, struct rtgcwii_fs* out) {
    struct rtsrc_sample p;
    int32_t s;
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
        rtsrc_latest(&w->src, channel, &p);
        s = -p.axes[2] * RTGCWII_TILT_SIN / RTHOME_AXIS_FULL;
        out->acc[2] = s;
        out->acc[1] = -rtgcwii_isqrt(RTHOME_AXIS_FULL * RTHOME_AXIS_FULL - s * s);
    }
    if (w->shake[channel] != 0) {
        rtgcwii_shake(w->shake[channel], (int)((w->shake_jump >> channel) & 1u), out->acc, &out->acc_value,
                      &out->acc_speed);
        w->shake_jump &= ~(1u << channel);  /* this read took the step's jump */
    }
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

int rtgcwii_next_change(struct rtgcwii* w, uint32_t skip, uint32_t keep, uint32_t* channel, int32_t* reason) { return rtslot_next_change(&w->slot, skip, keep, channel, reason); }

void rtgcwii_kpad_read(struct rtgcwii* w, uint32_t channel, int32_t result, int32_t error, uint32_t now) { rtslot_kpad_read(&w->slot, channel, result, error, now); }

void rtgcwii_still_told(struct rtgcwii* w, uint32_t channel) { rtslot_still_told(&w->slot, channel); }

void rtgcwii_not_told(struct rtgcwii* w, uint32_t channel) { rtslot_not_told(&w->slot, channel); }

void rtgcwii_heard(struct rtgcwii* w, uint32_t channel) { rtslot_heard(&w->slot, channel); }

void rtgcwii_retell(struct rtgcwii* w, uint32_t now) { rtslot_retell(&w->slot, now); }

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
        const uint32_t jump = w->shake_jump;  /* a raw report has no speed: the next KPAD status keeps it */
#ifdef RT_PAD_FORM_SIDEWAYS
        (void)now;
        rtgcwii_sideways(w, channel, &f);
#else
        rtgcwii_nunchuk(w, channel, now, &f);
#endif
        /* A format with an extension's part gets a report that long; on the
         * sideways Wii Remote, which has none, that part stays zeros with
         * the device type a bare Wii Remote (as WPAD gives a Wii Remote
         * with nothing in its port), so the success it was told matches
         * the report (CodeRabbit on #616). */
        if (format >= RTGCWII_FMT_FS_FIRST && format <= RTGCWII_FMT_FS_ACC_DPD) bytes = RTGCWII_WPAD_FS_BYTES;
        w->worked[channel] = worked;
        w->delivered[channel] = delivered;
        w->trig[channel] = trig;
        w->release[channel] = release;
        w->shake_jump = jump;
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

uint32_t rtgcwii_unified(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t entry_bytes, uint32_t count,
                         uint32_t now, uint32_t sample_ticks, int kpad_camera) {
    uint8_t first[RTGCWII_WPAD_FS_BYTES];
    uint32_t i, k, n;
    if (channel >= RTGCWII_CHANNELS || entry_bytes < RTGCWII_WPAD_FS_BYTES + 2u || count == 0) return 0;
    n = rtgcwii_wpad_status(w, channel, first, now, kpad_camera);
    if (n == 0) return 0;
    for (i = 0; i < count; ++i) {
        uint8_t* e = out + i * entry_bytes;
        for (k = 0; k < entry_bytes; ++k) e[k] = 0;
        for (k = 0; k < n; ++k) e[k] = first[k];
        e[entry_bytes - 2u] = (uint8_t)w->format[channel];
        /* The shake at each sample's own time: a step every shake_flip
         * ticks from when it began; a sample from before it, at rest. */
        if (w->shake[channel] != 0 && w->shake_flip != 0 && w->shake_from[channel] != 0) {
            int32_t acc[3], value, speed;
            const uint32_t since = rtgcwii_elapsed(now, w->shake_from[channel]);
            if (i * sample_ticks <= since) {
                const uint32_t steps = (since - i * sample_ticks) / w->shake_flip;
                rtgcwii_shake(RTGCWII_SHAKE_FLIP * steps + 1u, 0, acc, &value, &speed);
            } else {
                acc[0] = 0;
                acc[1] = -RTHOME_AXIS_FULL;
                acc[2] = 0;
            }
            rtgcwii_put_acc(e + 2, acc);
            /* The Nunchuk's too, where the sample has its part (it shakes
             * with the Wii Remote, rtgcwii_nunchuk), from the shake's start. */
            if (n == RTGCWII_WPAD_FS_BYTES && i * sample_ticks <= since) rtgcwii_put_acc(e + 42, acc);
        }
    }
    return count;
}
#else
uint32_t rtgcwii_unified(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t entry_bytes, uint32_t count,
                         uint32_t now, uint32_t sample_ticks, int kpad_camera) {
    (void)w;
    (void)channel;
    (void)out;
    (void)entry_bytes;
    (void)count;
    (void)now;
    (void)sample_ticks;
    (void)kpad_camera;
    return 0;
}
uint32_t rtgcwii_wpad_status(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t now, int kpad_camera) {
    (void)w;
    (void)channel;
    (void)out;
    (void)now;
    (void)kpad_camera;
    return 0;
}
#endif
