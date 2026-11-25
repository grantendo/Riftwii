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
#include "rtshape.h"

#include "rthome.h"

/* Ticks since a time noted with its low bit set (so 0 can mean "none"). */
static uint32_t rtgcwii_elapsed(uint32_t now, uint32_t noted) { return now - (noted & ~1u); }

uint32_t rtgcwii_map_nunchuk(const uint8_t* pad) {
    struct rtsrc_sample p;
    rtsrc_decode(pad, &p);
    return rtshape_nunchuk_buttons(&p);
}

uint32_t rtgcwii_map_sideways(const uint8_t* pad) {
    struct rtsrc_sample p;
    rtsrc_decode(pad, &p);
    return rtshape_sideways_buttons(&p);
}

void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now) {
    uint32_t i;
    for (i = 0; i < RTGCWII_CHANNELS; ++i) {
        const uint32_t bit = 1u << i;
        const enum rtsrc_event event = rtsrc_port(&w->src, i, status + i * RTGCWII_PAD_BYTES, w->slot.grace, now);
        if (event == RTSRC_ARRIVED || event == RTSRC_REPORT) {
            struct rtsrc_sample p;
            rtsrc_latest(&w->src, i, &p);
#if defined(RT_PAD_FORM_NUNCHUK)
            w->hold[i] = rtshape_nunchuk_buttons(&p);
#elif defined(RT_PAD_FORM_SIDEWAYS)
            w->hold[i] = rtshape_sideways_buttons(&p);
#else
            w->hold[i] = rtshape_classic_buttons(&p);
#endif
            rtshape_step(&w->shape, i, &p, now);
            /* Newly plugged in: what it holds already isn't newly pressed. */
            if (event == RTSRC_ARRIVED) w->delivered[i] = w->hold[i];
            w->slot.present |= bit;
        } else if (event == RTSRC_GONE) {
            w->slot.present &= ~bit;
            w->hold[i] = w->delivered[i] = 0;
            rtshape_reset(&w->shape, i);
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

void rtgcwii_nunchuk(struct rtgcwii* w, uint32_t channel, uint32_t now, struct rtshape_out* out) {
    struct rtsrc_sample p;
    rtshape_rest(out);
    if (!rtgcwii_on(w, channel)) {
        out->hold = out->trig = out->release = 0;
        return;
    }
    rtgcwii_words(w, channel, &out->hold, &out->trig, &out->release);
    rtsrc_latest(&w->src, channel, &p);
    rtshape_nunchuk(&w->shape, channel, &p, now, out);
}

void rtgcwii_sideways(struct rtgcwii* w, uint32_t channel, struct rtshape_out* out) {
    struct rtsrc_sample p;
    rtshape_rest(out);
    if (!rtgcwii_on(w, channel)) {
        out->hold = out->trig = out->release = 0;
        return;
    }
    rtgcwii_words(w, channel, &out->hold, &out->trig, &out->release);
    rtsrc_latest(&w->src, channel, &p);
    rtshape_sideways(&w->shape, channel, &p, out);
}

void rtgcwii_camera(struct rtgcwii* w, uint32_t channel, uint32_t mode) { rtshape_camera(&w->shape, channel, mode); }

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
    struct rtshape_out f;
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
        const uint32_t jump = w->shape.shake_jump;  /* a raw report has no speed: the next KPAD status keeps it */
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
        w->shape.shake_jump = jump;
    }
    for (i = 0; i < bytes; ++i) out[i] = 0;
    rtgcwii_put16(out, (int32_t)(f.hold & 0xFFFFu));
    rtgcwii_put_acc(out + 2, f.acc);
    if ((w->shape.camera >> channel) & 1u) {
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
        {
            int32_t acc[3];
            const int shaking = rtshape_shake_at(&w->shape, channel, now, i * sample_ticks, acc);
            if (shaking != 0) rtgcwii_put_acc(e + 2, acc);
            /* The Nunchuk's too, where the sample has its part (it shakes
             * with the Wii Remote, rtshape_nunchuk), from the shake's start. */
            if (shaking == 2 && n == RTGCWII_WPAD_FS_BYTES) rtgcwii_put_acc(e + 42, acc);
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
