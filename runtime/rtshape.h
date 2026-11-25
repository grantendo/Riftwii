/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shapes, the second of the stand-in's four layers (sources, shapes, slot
 * ownership, delivery; notes in #587): a controller's samples (rtsrc) as
 * the Wii controller the game expects. Each shape is a button layout plus
 * whatever motion the Wii controller has that the controller doesn't: the
 * shake (a button held), the pointer (the C-stick) and the sideways Wii
 * Remote's tilt. Only the shapes know a layout; ownership and delivery
 * never look at a controller's buttons. The build picks the shape today
 * (RT_PAD_FORM_NUNCHUK, RT_PAD_FORM_SIDEWAYS, else the Classic Controller);
 * the layouts are tables a pack's own layout file can fill later. Free of
 * the hardware, so the host tests run it. */
#ifndef RIFTWII_RTSHAPE_H
#define RIFTWII_RTSHAPE_H

#include <stdint.h>

#include "rtsrc.h"

#define RTSHAPE_PORTS RTSRC_PORTS

/* The shake: all three axes swing between +RTSHAPE_SHAKE_G and -SHAKE_G,
 * a step every `shake_flip` ticks while its button is held (the loader's
 * 40 ms, about the 2-3 frames a half-swing of Dolphin's emulated 15 Hz
 * shake takes; with no shake_flip, every RTSHAPE_SHAKE_FLIP samples), and
 * every RTSHAPE_SHAKE_STEPS-th step back at rest (1 g along -Y), as a
 * hand's shake passes through still: Donkey Kong Country Returns' "shake
 * to start" took Dolphin's shake, which does, but not a swing that never
 * came back below 3 g. 3.4 g is past the threshold Batman: The Brave and
 * the Bold checks for its heavy attacks (3.2 g for the Wii Remote, 2.0 g
 * for the Nunchuk). Numerators over RTHOME_AXIS_FULL (256: 1 g). */
#define RTSHAPE_SHAKE_G 870
#define RTSHAPE_SHAKE_VALUE 1508  /* |(3.4, 3.4, 3.4)| g */
#define RTSHAPE_SHAKE_FLIP 4u
#define RTSHAPE_SHAKE_STEPS 5u  /* +, -, +, -, at rest */
/* Full C-stick moves the pointer this much a sample (the screen is two
 * RTHOME_POINTER_FULL wide, so about a second across), and it stays inside
 * +-RTSHAPE_POINTER_EDGE, so the cursor never leaves the screen. */
#define RTSHAPE_POINTER_STEP 143
#define RTSHAPE_POINTER_EDGE 3891
/* The sideways form's tilt: the C-stick held right turns the Wii Remote as
 * a wheel steering right, gravity from -Y towards -Z (Dolphin's emulated
 * sideways Wii Remote, Excite Truck's tutorial, 22 Nov): full C-stick is
 * sin 55 degrees of it, over RTHOME_AXIS_FULL. */
#define RTSHAPE_TILT_SIN 210

/* The motion each slot's shape keeps between samples. */
struct rtshape {
    uint32_t pointer_show;                  /* ticks the pointer stays shown after the C-stick (loader-filled) */
    int32_t pointer[RTSHAPE_PORTS][2];      /* each slot's pointer, x and y over RTHOME_POINTER_FULL */
    uint32_t pointer_moved[RTSHAPE_PORTS];  /* when the C-stick last moved it (low bit set), 0: never */
    uint32_t shake[RTSHAPE_PORTS];          /* the shake's count while its button is held, 0: not held */
    /* The shake's rhythm by time, not by samples (a game that reads more
     * often would shake faster): when its button went down (low bit set),
     * which slots stepped since a read last took it, and the ticks between
     * steps (loader-filled; 0: every RTSHAPE_SHAKE_FLIP samples). */
    uint32_t shake_from[RTSHAPE_PORTS];
    uint32_t shake_jump;
    uint32_t shake_flip;
    uint32_t camera;  /* slots whose Wii Remote camera the game wants on (KPAD's switch), one bit each */
};

/* A Wii controller at one moment, in the Wii's own terms: the buttons,
 * the Nunchuk's stick, both accelerometers and the pointer, all
 * numerators over RTHOME_AXIS_FULL but the pointer's (over
 * RTHOME_POINTER_FULL). The words (hold, trig, release) are delivery's:
 * newly pressed is against what the game last got. */
struct rtshape_out {
    uint32_t hold, trig, release;
    int32_t stick[2];
    int32_t acc[3], acc_value, acc_speed;           /* the Wii Remote's */
    int32_t fs_acc[3], fs_acc_value, fs_acc_speed;  /* the Nunchuk's */
    int32_t pointer[2];
    int pointer_shown;  /* the game has the camera on, or the C-stick moved it within pointer_show ticks */
};

/* The buttons in each shape's default layout (#587), as KPADStatus' hold
 * word has them. Classic: the control stick and C-stick as the two
 * sticks, A B X Y as a b x y, L and R as themselves, Z as ZR, Start as +,
 * the D-pad as the D-pad. Wii Remote + Nunchuk: A and B as A and B, Y as
 * 2, Z as -, Start as +, the D-pad as the D-pad, L as the Nunchuk's Z and
 * R as its C (each also from its analog trigger, pushed past a third of
 * the way, as a Nunchuk's buttons need no hard press); X is the shake.
 * Sideways: A as 2, B as 1, X as A, Y as B, Z as -, Start as +, and the
 * D-pad and the control stick (past half way) as the Wii Remote's D-pad
 * turned as the screen shows it held sideways (D-pad on the left: screen
 * left is the remote's Up, right its Down, up its Right, down its Left);
 * R is the shake, L nothing. */
uint32_t rtshape_classic_buttons(const struct rtsrc_sample* p);
uint32_t rtshape_nunchuk_buttons(const struct rtsrc_sample* p);
uint32_t rtshape_sideways_buttons(const struct rtsrc_sample* p);

/* Once a sample, the build's shape's motion on `port`: the pointer follows
 * the C-stick (in the sideways shape only while the game has the camera
 * on: else the C-stick tilts), and the shake counts while its button is
 * held. Nothing in the Classic shape. */
void rtshape_step(struct rtshape* sh, uint32_t port, const struct rtsrc_sample* p, uint32_t now);

/* `port`'s controller unplugged: its shake and pointer's last move are
 * forgotten (an old mark never reads as recent after the time base wraps). */
void rtshape_reset(struct rtshape* sh, uint32_t port);

/* The game wants `port`'s Wii Remote camera on (`mode` not 0) or off, as
 * KPAD's switch says (KPADEnableDPD, KPADDisableDPD): a game turns it on
 * for its pointer menus (Batman: The Brave and the Bold from its title
 * screen on), so the pointer is shown while it is on, as well as after the
 * C-stick moves. */
void rtshape_camera(struct rtshape* sh, uint32_t port, uint32_t mode);

/* A controller held still with nothing pressed: both accelerometers 1 g
 * along -Y (as KPAD reports a real one at rest), stick and pointer centred,
 * the pointer hidden. The words are left alone. */
void rtshape_rest(struct rtshape_out* out);

/* The Wii Remote + Nunchuk's motion on `port` from sample `p` at `now`
 * into `out` (the words are left alone): the Nunchuk's stick, the pointer,
 * and both accelerometers shaking together while the shake's button is
 * held. A read takes the shake's step (its jump in speed). */
void rtshape_nunchuk(struct rtshape* sh, uint32_t port, const struct rtsrc_sample* p, uint32_t now,
                     struct rtshape_out* out);

/* The sideways Wii Remote's on `port`: the pointer while the game has the
 * camera on, else the tilt the C-stick gives; the shake while its button is
 * held. The Nunchuk's fields stay at rest. */
void rtshape_sideways(struct rtshape* sh, uint32_t port, const struct rtsrc_sample* p, struct rtshape_out* out);

/* The Wii Remote's acceleration on `port` `ago` ticks before `now`, for a
 * delivery that needs past moments (KPAD's raw samples, each at its own
 * time): 0 when no time-kept shake is held (nothing written); 1 when one
 * is but that moment was before it began (`acc` at rest); 2 with `acc` at
 * the shake's step then. */
int rtshape_shake_at(const struct rtshape* sh, uint32_t port, uint32_t now, uint32_t ago, int32_t acc[3]);

#endif
