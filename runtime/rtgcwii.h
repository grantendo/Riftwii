/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GameCube controllers as Classic Controllers (#587): with the game's own
 * setting on (off by default), each GameCube controller in port N stands in
 * on Wii Remote channel N-1 as a Classic Controller for as long as it is
 * plugged in, so a game that takes a Classic Controller plays from it. The
 * HOME Menu's stand-in (rthome.h) does the same for a moment; this one stays.
 *
 * The decisions, kept free of the hardware so the host tests can run them;
 * the pad blob (runtime/pad) calls them from its PADRead, KPAD read and
 * WPADProbe hooks, and writes the Classic Controller's status into the
 * game's KPAD buffer as it does for the HOME Menu's stand-in. The state
 * lives in MEM2 next to the adapter driver's (the blob's context has no
 * room for four ports), and the context holds its address. */
#ifndef RIFTWII_RTGCWII_H
#define RIFTWII_RTGCWII_H

#include <stdint.h>

#include "rtslot.h"
#include "rtsrc.h"
#include "rtshape.h"
#include "rtdeliver.h"

#define RTGCWII_CHANNELS 4u
#define RTGCWII_PAD_BYTES 12u

struct rtgcwii {
    struct rtslot slot;  /* who holds each Wii Remote slot, and what the game was told (rtslot.h) */
    struct rtsrc src;    /* each player's controller, one sample per report (rtsrc.h) */
    uint32_t hold[RTGCWII_CHANNELS];       /* each channel's Wii buttons now, in its shape */
    struct rtdeliver deliver;  /* what the game's KPAD read last got on each slot (rtdeliver.h) */
    /* Many games that take a Classic Controller never call PADRead (Kamen
     * Rider: Dragon Knight reads only through KPAD), so the adapter would
     * never be read: the blob then calls the game's PADRead itself, once a
     * frame, as that game's pack did. */
    uint32_t game_read;  /* when the game itself last called PADRead (low bit set), 0: never */
    uint32_t own_read;   /* when the blob last called it (low bit set), 0: never */
    uint32_t reading;    /* the blob's own PADRead call is under way */
    /* A real controller's KPAD reads with no error output that brought no
     * samples: since when (low bit set; 0: the last read brought some) and
     * how many in a row. */
    /* When a real controller's channels were last checked with the game's
     * own WPADProbe (low bit set), 0: never (rtgcwii_real_check_due). */
    struct rtshape shape;  /* each slot's shape's motion: shake, pointer, camera (rtshape.h) */
    /* Where KPAD keeps each channel's camera switch (KPADEnableDPD's
     * code): the byte at kpad_dpd + channel * kpad_dpd_stride +
     * kpad_dpd_offset, 0: not found (the pointer shows only after the
     * C-stick moves). Loader-filled. */
    uint32_t kpad_dpd, kpad_dpd_stride, kpad_dpd_offset;
    /* The HOME Menu's stand-in's channel (one bit) when the game knew it
     * as this stand-in's as the HOME stand-in took it, 0: a Classic one.
     * It keeps that shape until the next time it takes a channel, though
     * the controller unplugs under the menu (the Wii Remote forms only). */
    /* A 2006 KPAD game's WPADGetInfoAsync callback, answered for the
     * stand-in and told at the game's next KPAD read on that channel (as a
     * real Wii Remote's answer comes later), 0: none waiting. */
    uint32_t info_callback[RTGCWII_CHANNELS];
    /* 1: a 2006 KPAD game, whose WPADGetInfoAsync on the stand-in's channel
     * gets the stand-in's info (pad_on_info); 0: a later game, where it is
     * busy (-2), so a blocking WPADGetInfo (Mario Strikers Charged's, after
     * its title) returns at once instead of sleeping for a reply only a real
     * Wii Remote sends; the game skips its battery check. Loader-filled. */
    uint32_t info_answers;
    /* The 2006 WPAD's connect callbacks (its games have no KPAD-level
     * one): the table of the channels' control blocks and the callback's
     * place in one, 0: none. Loader-filled. */
    uint32_t wpad_connect_table, wpad_connect_offset;
    /* The stand-in below KPAD (the Wii Remote forms), for a game that reads
     * WPAD itself (Punch-Out!!: WPADRead into its own pad manager): the data
     * format the game last asked for on each channel (WPADSetDataFormat,
     * answered for the stand-in), 0 until it asks. */
    uint32_t format[RTGCWII_CHANNELS];
    /* Telling such a game again: it may hear "connected" before its own
     * callback is set (Punch-Out!!'s WPAD callback passes it on to one the
     * game sets later), so a channel told and not read since (no KPAD read
     * or WPADRead given the stand-in there) is told again every
     * retell_ticks, at most RTGCWII_RETELLS times. retell_ticks 0: never
     * (loader-filled, only where WPADRead is hooked). */
    /* Channels due to be told "connected" again (rtgcwii_retell), one bit
     * each: apart from `told`, which stays set, so a controller unplugged
     * before the telling is still told it's gone. */
    /* wpad_connect_table is the later WPAD's (find_wpad_connect_later): the
     * game's sleeps read the pads for the stand-in only while the game
     * itself is quiet (rtgcwii_quiet_due, pad_hook.c's pad_reads_at_sleep).
     * Loader-filled. */
    uint32_t wpad_connect_later;
    /* When the game last read a Wii controller through a hook (its KPAD
     * read, WPADProbe or WPADRead; low bit set), 0: never; and when the
     * first sleep that asked was (low bit set), 0: none yet. */
    /* Quiet already reached: kept until the next Wii controller read, so the
     * time base wrapping round can't make an old timestamp look recent. */
    /* WPAD's extension callbacks (the game's, WPADSetExtensionCallback):
     * the table of the channels' control blocks and the callback's place in
     * one (just before the connect callback's), 0: none. Told the stand-in's
     * attachment after its "connected" (pad_tell_extension). Loader-filled. */
    uint32_t wpad_ext_table, wpad_ext_offset;
};

#define RTGCWII_RETELLS RTSLOT_RETELLS

/* At each PADRead, the four ports' statuses (12 bytes each) as the game
 * will see them (the adapter's already in): a port with a controller keeps
 * its status and its Classic buttons move on one sample; a port with none
 * for `grace` ticks loses its channel's stand-in. A short gap (a wireless
 * pad's dropped report) never unplugs the player. */
void rtgcwii_pads(struct rtgcwii* w, const uint8_t* status, uint32_t now);

/* WPADProbe(`channel`) answered `result` (0 a controller, -1 none, -2
 * busy) before the blob's own answer: a real controller there wins over
 * the stand-in. When it goes again, the stand-in is announced afresh (the
 * game's library told the game that channel's controller left). */
void rtgcwii_probed(struct rtgcwii* w, uint32_t channel, int32_t result);

/* How many probe results `channel` has had (rtgcwii_probed counts them):
 * taken before a probe whose answer is applied later, so a newer result in
 * between isn't undone by the older one. */
uint32_t rtgcwii_probe_count(const struct rtgcwii* w, uint32_t channel);

/* Whether the channels a real controller holds are due a check of their
 * own (the game's WPADProbe, answered through rtgcwii_probed): some games
 * stop reading a channel once their library reports it gone, so its
 * hooked reads and probes never tell the stand-in the Wii Remote turned
 * off, and the GameCube controller would stay locked out. Every 250 ms
 * while a channel is real; `now` is noted when it is due. */
int rtgcwii_real_check_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms);

/* Whether the stand-in answers on `channel` now. */
int rtgcwii_on(const struct rtgcwii* w, uint32_t channel);

/* The channels the game knows as this stand-in's: held now, told of and
 * not yet told gone, and the HOME Menu's stand-in's own when it took one
 * of these. */
uint32_t rtgcwii_shaped(const struct rtgcwii* w);

/* The HOME Menu's stand-in takes `channel` (RTGCWII_CHANNELS or more:
 * none): the shape it takes it in (this stand-in's, or a Classic
 * Controller's) is kept until it next takes one, whatever is plugged in or
 * unplugged meanwhile (Cake's review of #610). */
void rtgcwii_home_took(struct rtgcwii* w, uint32_t channel);

/* Whether the HOME Menu's stand-in on `channel` has this stand-in's shape. */
int rtgcwii_home_keeps_shape(const struct rtgcwii* w, uint32_t channel);

/* The game's connect callback: the channels to report gone (*leave) and
 * connected (*join), one bit each, since it was last told. A channel a real
 * controller took is neither: the game's own library tells it about that. */
void rtgcwii_connect_change(struct rtgcwii* w, uint32_t* leave, uint32_t* join);

/* The Wii Remote + Nunchuk form's buttons (KPADStatus' hold word) for a
 * PADStatus, in #587's default Nunchuk layout: A and B as A and B, Y as 2,
 * Z as -, Start as +, the D-pad as the D-pad, L as the Nunchuk's Z and R as
 * its C (each also from its analog trigger, pushed past a third of the
 * way, as a Nunchuk's buttons need no hard press). X is the shake
 * (rtgcwii_nunchuk). In every build; RT_PAD_FORM_NUNCHUK builds take it. */
uint32_t rtgcwii_map_nunchuk(const uint8_t* pad);

/* The sideways Wii Remote form's buttons (KPADStatus' hold word) for a
 * PADStatus, in #587's default sideways layout: A as 2, B as 1, X as A, Y as
 * B, Z as -, Start as +, and the D-pad and the control stick (past half way)
 * as the Wii Remote's D-pad turned as the screen shows it held sideways
 * (D-pad on the left: screen left is the remote's Up, right its Down, up its
 * Right, down its Left). R is the shake, L nothing (rtgcwii_sideways). In
 * every build; RT_PAD_FORM_SIDEWAYS builds take it. */
uint32_t rtgcwii_map_sideways(const uint8_t* pad);

/* The shapes' constants (rtshape.h), by their old names. */
#define RTGCWII_SHAKE_G RTSHAPE_SHAKE_G
#define RTGCWII_SHAKE_VALUE RTSHAPE_SHAKE_VALUE
#define RTGCWII_SHAKE_FLIP RTSHAPE_SHAKE_FLIP
#define RTGCWII_SHAKE_STEPS RTSHAPE_SHAKE_STEPS
#define RTGCWII_POINTER_STEP RTSHAPE_POINTER_STEP
#define RTGCWII_POINTER_EDGE RTSHAPE_POINTER_EDGE
#define RTGCWII_TILT_SIN RTSHAPE_TILT_SIN

/* The game wants `channel`'s Wii Remote camera on (`mode` not 0) or off,
 * as KPAD's switch says (KPADEnableDPD, KPADDisableDPD): a game turns it on
 * for its pointer menus (Batman: The Brave and the Bold from its title
 * screen on), so the stand-in's pointer is shown while it is on, as well as
 * after the C-stick moves. */
void rtgcwii_camera(struct rtgcwii* w, uint32_t channel, uint32_t mode);

#define rtgcwii_float_bits rtdeliver_float_bits

/* The stand-in's Wii Remote + Nunchuk on `channel` (RT_PAD_FORM_NUNCHUK),
 * for the game's KPAD read at `now`: the same words for every read until
 * the next sample, as rtgcwii_classic gives. Holding X shakes the Wii
 * Remote's accelerometer and the Nunchuk's together. Where the stand-in
 * isn't on, the buttons, stick and pointer are 0 (pointer hidden), and both
 * accelerometers read a still controller: 1 g along -Y. */
void rtgcwii_nunchuk(struct rtgcwii* w, uint32_t channel, uint32_t now, struct rtshape_out* out);

/* The stand-in's Wii Remote on `channel` in the build's shape (with a
 * Nunchuk, or sideways) at `now`, into `out`. The game's own KPAD read
 * takes the words (RTGCWII_TAKE_WORDS: newly pressed and let go against
 * what it last got) and the shake's step (RTGCWII_TAKE_STEP: its jump in
 * speed); a look takes neither and leaves them for that read: a raw report
 * (it has neither) takes 0, and the HOME Menu's stand-in's status in this
 * shape, whose buttons are its own, takes the step alone. */
#define RTGCWII_TAKE_WORDS 1u
#define RTGCWII_TAKE_STEP 2u
void rtgcwii_wii_remote(struct rtgcwii* w, uint32_t channel, uint32_t now, struct rtshape_out* out, uint32_t takes);

/* The stand-in's sideways Wii Remote on `channel` (RT_PAD_FORM_SIDEWAYS),
 * for the game's KPAD read: the words, the acceleration (tilted by the
 * C-stick's left and right while the game has the camera off, shaking while
 * R is held) and, while the game has the camera on, the pointer the C-stick
 * moves. The Nunchuk's fields stay at rest. Where the stand-in isn't on,
 * the buttons and pointer are 0 and the accelerometer reads a still
 * controller: 1 g along -Y. */
void rtgcwii_sideways(struct rtgcwii* w, uint32_t channel, struct rtshape_out* out);

/* The stand-in's Classic Controller on `channel`, for the game's KPAD read:
 * its hold, trig (newly pressed since the game last got it) and release
 * words, the same for every read until the next sample, and its sticks and
 * triggers as numerators over RTHOME_AXIS_FULL
 * (rthome_classic_axes). The default layout of #587: the control stick and
 * C-stick as the two sticks, A B X Y as a b x y, L and R as themselves, Z
 * as ZR, Start as +, the D-pad as the D-pad. */
void rtgcwii_classic(struct rtgcwii* w, uint32_t channel, uint32_t* hold, uint32_t* trig, uint32_t* release,
                     int32_t axes[6]);

/* Its buttons alone, for a raw status (KPADGetUnifiedWpadStatus), which
 * moves nothing on; 0 where the stand-in doesn't answer. */
uint32_t rtgcwii_hold(const struct rtgcwii* w, uint32_t channel);

/* The connect callback could not be told `channel` joined (the game hasn't
 * set one yet): it is told again at the next PADRead. */
void rtgcwii_not_told(struct rtgcwii* w, uint32_t channel);

/* The same for "gone": it stays to be told (the channel counts as told
 * connected until then). */
void rtgcwii_still_told(struct rtgcwii* w, uint32_t channel);

/* The next change the game's connect callback should hear, one at a time
 * from what is true now (so a PADRead nested in another, from an SI
 * callback, can't leave a stale "gone" last): 1 with `channel` and
 * `reason` (0 connected, -1 gone), noted as told; 0 when there is none.
 * Channels in `skip` (one bit each) are passed over this time; channels in
 * `keep` are never reported gone (the HOME Menu's stand-in has them now,
 * and its own end tells the game). */
int rtgcwii_next_change(struct rtgcwii* w, uint32_t skip, uint32_t keep, uint32_t* channel, int32_t* reason);

/* A KPAD read on `channel` returned `result` samples, `error` its WPAD
 * error where the read gives one (RTGCWII_NO_ERROR otherwise): samples mean
 * a real controller is there (it wins, before any WPADProbe says so); "no
 * controller" (-1) means it left. A read with no error output can't say
 * so: there, a real controller whose reads bring no samples for `grace`
 * ticks and RTGCWII_EMPTY_READS reads in a row has left too (a game that
 * stops calling WPADProbe would otherwise keep the stand-in away). */
#define RTGCWII_NO_ERROR RTSLOT_NO_ERROR
#define RTGCWII_EMPTY_READS RTSLOT_EMPTY_READS
void rtgcwii_kpad_read(struct rtgcwii* w, uint32_t channel, int32_t result, int32_t error, uint32_t now);

/* At a KPAD read or WPADProbe: whether the blob should call the game's
 * PADRead now (and, if so, notes the time it does). Only when the game
 * hasn't called it itself within 50 ms, and at most every 8 ms (about once
 * a frame). Times are compared unsigned, and a read by the game that has
 * gone stale is forgotten: the time base's low word passes its halfway
 * mark every 35 s, and a signed difference from a read that long ago
 * turned negative and looked recent (Dragon Knight called PADRead once at
 * start-up, and its save screens then took no presses). */
int rtgcwii_should_read(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms);

/* PADRead ran at `now`; `own`: the blob's own call (rtgcwii_should_read),
 * else the game's. */
void rtgcwii_read_by(struct rtgcwii* w, uint32_t now, int own);

/* The game read the stand-in on `channel` (a KPAD read or WPADRead gave it):
 * it has heard of it, and isn't told again. */
void rtgcwii_heard(struct rtgcwii* w, uint32_t channel);

/* The game read a Wii controller through a hook (its KPAD read, WPADProbe
 * or WPADRead) at `now`. */
void rtgcwii_wii_read(struct rtgcwii* w, uint32_t now);

/* Whether a sleep should read the pads in a game whose connect callback is
 * the later WPAD's: only while the game itself is quiet, so one that reads
 * nothing until its callback hears of a controller (Mario Strikers
 * Charged) gets the read that announces the stand-in, and one that reads
 * WPAD every frame (LEGO Star Wars: The Complete Saga, which stopped at
 * start-up when its sleeps read the pads) never does. Quiet: no Wii
 * controller read in 3 s since its last, or none at all 5 s after the
 * first sleep that asked. */
int rtgcwii_quiet_due(struct rtgcwii* w, uint32_t now, uint32_t ticks_per_ms);

/* At each PADRead: a channel the game was told of and hasn't read since,
 * retell_ticks after the telling, is due to be told again (`retell`), so the
 * connect callback hears "connected" again (rtgcwii_next_change), at most
 * RTGCWII_RETELLS times while its controller stays. It still counts as
 * told: one that leaves first is told it's gone. */
void rtgcwii_retell(struct rtgcwii* w, uint32_t now);

/* The raw report's layout (rtdeliver.h), by its old names. */
#define RTGCWII_WPAD_CORE_BYTES RTDELIVER_WPAD_CORE_BYTES
#define RTGCWII_WPAD_FS_BYTES RTDELIVER_WPAD_FS_BYTES
#define RTGCWII_WPAD_ERR RTDELIVER_WPAD_ERR
#define RTGCWII_ACC_G RTDELIVER_ACC_G
#define RTGCWII_FS_STICK_FULL RTDELIVER_FS_STICK_FULL
#define RTGCWII_DOT_RANGE_X RTDELIVER_DOT_RANGE_X
#define RTGCWII_DOT_RANGE_Y RTDELIVER_DOT_RANGE_Y
#define RTGCWII_DOT_APART RTDELIVER_DOT_APART
#define RTGCWII_DOT_SIZE RTDELIVER_DOT_SIZE

/* The stand-in's raw status on `channel` (the Wii Remote forms) into `out`,
 * for a game that reads WPAD itself: its form's buttons and acceleration
 * (rtgcwii_sideways, rtgcwii_nunchuk) and, while the camera is on (KPAD's
 * switch, `kpad_camera`, or a data format with the camera that the game
 * asked for), the pointer as the two dots; the Nunchuk's part only when the
 * game asked for a Nunchuk format (WPADRead copies that format's size).
 * Returns the bytes written, 0 (none) where the stand-in isn't on. In the
 * Classic Controller form it writes nothing. */
uint32_t rtgcwii_wpad_status(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t now, int kpad_camera);
/* KPADGetUnifiedWpadStatus's raw samples for the stand-in's channel, newest
 * first: `count` entries of `entry_bytes` (the raw report, rtgcwii_wpad_status,
 * then the data format in the entry's last-but-one byte), each
 * `sample_ticks` older than the one before, so a shake swings through them
 * as a real Wii Remote's samples do (Donkey Kong Country Returns' "shake
 * to start" reads these, not KPAD's statuses). 0 when the channel isn't
 * the stand-in's (nothing written). */
uint32_t rtgcwii_unified(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t entry_bytes, uint32_t count,
                         uint32_t now, uint32_t sample_ticks, int kpad_camera);

#endif
