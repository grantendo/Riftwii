/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Samples as the Wii controller the game expects (rtshape.h). */
#ifndef RT_PAD_STANDIN
#define RT_PAD_STANDIN
#endif
#include "rtshape.h"

#include "rthome.h"

/* Ticks since a time noted with its low bit set (so 0 can mean "none"). */
static uint32_t rtshape_elapsed(uint32_t now, uint32_t noted) { return now - (noted & ~1u); }

/* A Wii Remote + Nunchuk's buttons in KPADStatus.hold. */
#define RTSHAPE_WR_LEFT 0x0001u
#define RTSHAPE_WR_RIGHT 0x0002u
#define RTSHAPE_WR_DOWN 0x0004u
#define RTSHAPE_WR_UP 0x0008u
#define RTSHAPE_WR_PLUS 0x0010u
#define RTSHAPE_WR_2 0x0100u
#define RTSHAPE_WR_1 0x0200u
#define RTSHAPE_WR_B 0x0400u
#define RTSHAPE_WR_A 0x0800u
#define RTSHAPE_WR_MINUS 0x1000u
#define RTSHAPE_NC_Z 0x2000u
#define RTSHAPE_NC_C 0x4000u
#define RTSHAPE_TRIGGER_ON 85u  /* of 255: an analog L or R past a third of the way */
#define RTSHAPE_STICK_PRESS 128  /* of RTHOME_AXIS_FULL: the control stick past half way is the D-pad */

uint32_t rtshape_classic_buttons(const struct rtsrc_sample* p) { return rthome_map_classic(p->buttons, RTHOME_CL_PLUS); }

uint32_t rtshape_nunchuk_buttons(const struct rtsrc_sample* p) {
    uint32_t b = p->buttons, h = 0;
    if (p->trigger[0] >= RTSHAPE_TRIGGER_ON) b |= RTSRC_L;
    if (p->trigger[1] >= RTSHAPE_TRIGGER_ON) b |= RTSRC_R;
    if (b & RTSRC_A) h |= RTSHAPE_WR_A;
    if (b & RTSRC_B) h |= RTSHAPE_WR_B;
    if (b & RTSRC_Y) h |= RTSHAPE_WR_2;
    if (b & RTSRC_Z) h |= RTSHAPE_WR_MINUS;
    if (b & RTSRC_START) h |= RTSHAPE_WR_PLUS;
    if (b & RTSRC_L) h |= RTSHAPE_NC_Z;
    if (b & RTSRC_R) h |= RTSHAPE_NC_C;
    if (b & RTSRC_LEFT) h |= RTSHAPE_WR_LEFT;
    if (b & RTSRC_RIGHT) h |= RTSHAPE_WR_RIGHT;
    if (b & RTSRC_UP) h |= RTSHAPE_WR_UP;
    if (b & RTSRC_DOWN) h |= RTSHAPE_WR_DOWN;
    return h;
}

uint32_t rtshape_sideways_buttons(const struct rtsrc_sample* p) {
    uint32_t b = p->buttons, h = 0;
    if (p->axes[0] <= -RTSHAPE_STICK_PRESS) b |= RTSRC_LEFT;
    if (p->axes[0] >= RTSHAPE_STICK_PRESS) b |= RTSRC_RIGHT;
    if (p->axes[1] >= RTSHAPE_STICK_PRESS) b |= RTSRC_UP;
    if (p->axes[1] <= -RTSHAPE_STICK_PRESS) b |= RTSRC_DOWN;
    if (b & RTSRC_A) h |= RTSHAPE_WR_2;
    if (b & RTSRC_B) h |= RTSHAPE_WR_1;
    if (b & RTSRC_X) h |= RTSHAPE_WR_A;
    if (b & RTSRC_Y) h |= RTSHAPE_WR_B;
    if (b & RTSRC_Z) h |= RTSHAPE_WR_MINUS;
    if (b & RTSRC_START) h |= RTSHAPE_WR_PLUS;
    /* Held sideways, D-pad on the left: the remote's Up points left. */
    if (b & RTSRC_LEFT) h |= RTSHAPE_WR_UP;
    if (b & RTSRC_RIGHT) h |= RTSHAPE_WR_DOWN;
    if (b & RTSRC_UP) h |= RTSHAPE_WR_RIGHT;
    if (b & RTSRC_DOWN) h |= RTSHAPE_WR_LEFT;
    return h;
}

#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
/* The shake's count at a sample: by time (every shake_flip ticks a step),
 * else by samples; shake_jump notes a step until a read (rtshape_nunchuk,
 * rtshape_sideways) takes it, so samples between two reads can't lose it
 * (CodeRabbit CLI, #610). */
static void rtshape_count_shake(struct rtshape* sh, uint32_t i, uint32_t shaking, uint32_t now) {
    const uint32_t bit = 1u << i;
    uint32_t flips;
    if (!shaking) {
        sh->shake[i] = 0;
        sh->shake_from[i] = 0;
        sh->shake_jump &= ~bit;
        return;
    }
    if (sh->shake_flip == 0) {
        ++sh->shake[i];
        if (sh->shake[i] > 1u && (sh->shake[i] % RTSHAPE_SHAKE_FLIP) == 0) sh->shake_jump |= bit;
        return;
    }
    if (sh->shake_from[i] == 0) sh->shake_from[i] = now | 1u;
    flips = rtshape_elapsed(now, sh->shake_from[i]) / sh->shake_flip;
    /* The count keeps the flips in its RTSHAPE_SHAKE_FLIP's (rtshape_shake
     * takes the side from it), never 0 while held. */
    {
        const uint32_t count = RTSHAPE_SHAKE_FLIP * flips + 1u;
        if (sh->shake[i] != 0 && count != sh->shake[i]) sh->shake_jump |= bit;
        sh->shake[i] = count;
    }
}
#endif

void rtshape_step(struct rtshape* sh, uint32_t i, const struct rtsrc_sample* s, uint32_t now) {
#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
    const int32_t* axes = s->axes;  /* the C-stick is axes 2 and 3 */
    int32_t k;
    uint32_t shaking;
    if (i >= RTSHAPE_PORTS) return;
#ifdef RT_PAD_FORM_SIDEWAYS
    /* R, also from the analog trigger. */
    shaking = (s->buttons & RTSRC_R) || s->trigger[1] >= RTSHAPE_TRIGGER_ON;
    rtshape_count_shake(sh, i, shaking, now);
    if (!((sh->camera >> i) & 1u)) return;
#else
    shaking = s->buttons & RTSRC_X;
#endif
    for (k = 0; k < 2; ++k) {
        /* Up on the C-stick is up on the screen (KPAD's y grows downward). */
        const int32_t step = (k == 0 ? axes[2] : -axes[3]) * RTSHAPE_POINTER_STEP / RTHOME_AXIS_FULL;
        int32_t p = sh->pointer[i][k] + step;
        if (p > RTSHAPE_POINTER_EDGE) p = RTSHAPE_POINTER_EDGE;
        if (p < -RTSHAPE_POINTER_EDGE) p = -RTSHAPE_POINTER_EDGE;
        sh->pointer[i][k] = p;
    }
    if (axes[2] != 0 || axes[3] != 0) {
        sh->pointer_moved[i] = now | 1u;
    } else if (sh->pointer_moved[i] != 0 && rtshape_elapsed(now, sh->pointer_moved[i]) >= sh->pointer_show) {
        /* Hidden for good until the C-stick moves again: the time base
         * wraps (about every 70 s), and an old mark would read as recent. */
        sh->pointer_moved[i] = 0;
    }
#ifndef RT_PAD_FORM_SIDEWAYS
    rtshape_count_shake(sh, i, shaking, now);
#endif
#else
    (void)sh;
    (void)i;
    (void)s;
    (void)now;
#endif
}

void rtshape_reset(struct rtshape* sh, uint32_t i) {
#if defined(RT_PAD_FORM_NUNCHUK) || defined(RT_PAD_FORM_SIDEWAYS)
    if (i >= RTSHAPE_PORTS) return;
    sh->shake[i] = 0;
    sh->shake_from[i] = 0;
    sh->shake_jump &= ~(1u << i);
    sh->pointer_moved[i] = 0;
#else
    (void)sh;
    (void)i;
#endif
}

void rtshape_camera(struct rtshape* sh, uint32_t port, uint32_t mode) {
    if (port >= RTSHAPE_PORTS) return;
    if (mode != 0) {
        sh->camera |= 1u << port;
    } else {
        sh->camera &= ~(1u << port);
    }
}

/* A controller held still: gravity along -Y, as KPAD reports a real one at
 * rest (both the Wii Remote's and the Nunchuk's accelerometers). */
static void rtshape_still(int32_t acc[3], int32_t* value, int32_t* speed) {
    acc[0] = 0;
    acc[1] = -RTHOME_AXIS_FULL;
    acc[2] = 0;
    *value = RTHOME_AXIS_FULL;
    *speed = 0;
}

void rtshape_rest(struct rtshape_out* out) {
    rtshape_still(out->acc, &out->acc_value, &out->acc_speed);
    rtshape_still(out->fs_acc, &out->fs_acc_value, &out->fs_acc_speed);
    out->stick[0] = out->stick[1] = 0;
    out->pointer[0] = out->pointer[1] = 0;
    out->pointer_shown = 0;
}

/* The shake: all three axes swing together, every RTSHAPE_SHAKE_STEPS-th
 * step at rest, and a step (`jump`, rtshape_count_shake) is a jump in
 * speed. */
static void rtshape_shake(uint32_t shake, int jump, int32_t acc[3], int32_t* value, int32_t* speed) {
    const uint32_t step = shake / RTSHAPE_SHAKE_FLIP;
    *speed = jump ? RTSHAPE_SHAKE_VALUE : 0;
    if (step % RTSHAPE_SHAKE_STEPS == RTSHAPE_SHAKE_STEPS - 1u) {
        acc[0] = 0;
        acc[1] = -RTHOME_AXIS_FULL;
        acc[2] = 0;
        *value = RTHOME_AXIS_FULL;
        return;
    }
    {
        const int32_t swing = (step & 1u) ? RTSHAPE_SHAKE_G : -RTSHAPE_SHAKE_G;
        acc[0] = swing;
        acc[1] = swing;
        acc[2] = -swing;
        *value = RTSHAPE_SHAKE_VALUE;
    }
}

/* The integer square root of `v` (rounded down). */
static int32_t rtshape_isqrt(int32_t v) {
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

void rtshape_nunchuk(const struct rtshape* sh, uint32_t port, const struct rtsrc_sample* p, uint32_t now,
                     struct rtshape_out* out) {
    uint32_t shake;
    rtshape_rest(out);
    if (port >= RTSHAPE_PORTS) return;
    out->stick[0] = p->axes[0];
    out->stick[1] = p->axes[1];
    out->pointer[0] = sh->pointer[port][0];
    out->pointer[1] = sh->pointer[port][1];
    out->pointer_shown = ((sh->camera >> port) & 1u) ||
                         (sh->pointer_moved[port] != 0 && rtshape_elapsed(now, sh->pointer_moved[port]) < sh->pointer_show);
    shake = sh->shake[port];
    if (shake != 0) {
        rtshape_shake(shake, (int)((sh->shake_jump >> port) & 1u), out->acc, &out->acc_value, &out->acc_speed);
    }
}

void rtshape_sideways(const struct rtshape* sh, uint32_t port, const struct rtsrc_sample* p, struct rtshape_out* out) {
    int32_t s;
    rtshape_rest(out);
    if (port >= RTSHAPE_PORTS) return;
    if ((sh->camera >> port) & 1u) {
        out->pointer[0] = sh->pointer[port][0];
        out->pointer[1] = sh->pointer[port][1];
        out->pointer_shown = 1;
    } else {
        /* Tilt: the C-stick held right steers right, gravity turning from
         * -Y towards -Z, its length staying 1 g. */
        s = -p->axes[2] * RTSHAPE_TILT_SIN / RTHOME_AXIS_FULL;
        out->acc[2] = s;
        out->acc[1] = -rtshape_isqrt(RTHOME_AXIS_FULL * RTHOME_AXIS_FULL - s * s);
    }
    if (sh->shake[port] != 0) {
        rtshape_shake(sh->shake[port], (int)((sh->shake_jump >> port) & 1u), out->acc, &out->acc_value,
                      &out->acc_speed);
    }
}

void rtshape_take_step(struct rtshape* sh, uint32_t port) {
    if (port < RTSHAPE_PORTS) sh->shake_jump &= ~(1u << port);
}
