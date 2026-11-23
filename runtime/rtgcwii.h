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

#define RTGCWII_CHANNELS 4u
#define RTGCWII_PAD_BYTES 12u

struct rtgcwii {
    uint32_t grace;    /* ticks a port may report no controller before its channel's goes (loader-filled) */
    uint32_t present;  /* channels with a GameCube controller behind them, one bit each */
    uint32_t real;     /* channels where WPADProbe last found a real controller: the stand-in steps aside */
    uint32_t told;     /* channels the game's connect callback last heard are connected (by this) */
    uint32_t seen[RTGCWII_CHANNELS];  /* when each port last had a controller (low bit set), 0: gone */
    uint32_t hold[RTGCWII_CHANNELS];       /* each channel's Classic buttons now */
    uint32_t samples[RTGCWII_CHANNELS];    /* PADRead samples taken for it */
    /* What the game's KPAD read last got (newly pressed and let go are
     * against that, not the sample before: a sample the blob takes at a
     * WPADProbe the game never reads in between, and its press still
     * counts), worked out once per sample so a second read in a frame
     * sees the same words. */
    uint32_t worked[RTGCWII_CHANNELS];     /* the sample the words below are from */
    uint32_t delivered[RTGCWII_CHANNELS];  /* the buttons the game last got */
    uint32_t trig[RTGCWII_CHANNELS], release[RTGCWII_CHANNELS];
    uint8_t pad[RTGCWII_CHANNELS][RTGCWII_PAD_BYTES];  /* each port's PADStatus as the last PADRead had it */
    /* Many games that take a Classic Controller never call PADRead (Kamen
     * Rider: Dragon Knight reads only through KPAD), so the adapter would
     * never be read: the blob then calls the game's PADRead itself, once a
     * frame, as that game's pack did. */
    uint32_t game_read;  /* when the game itself last called PADRead (low bit set), 0: never */
    uint32_t own_read;   /* when the blob last called it (low bit set), 0: never */
    uint32_t reading;    /* the blob's own PADRead call is under way */
    uint32_t telling;    /* the game's connect callback is being told (nested PADReads leave it to that) */
    /* A real controller's KPAD reads with no error output that brought no
     * samples: since when (low bit set; 0: the last read brought some) and
     * how many in a row. */
    uint32_t empty_since[RTGCWII_CHANNELS];
    uint32_t empty_reads[RTGCWII_CHANNELS];
    /* The Wii Remote + Nunchuk form (RT_PAD_FORM_NUNCHUK; unused by the
     * Classic one, but every build has the same layout, which the loader
     * fills). */
    uint32_t pointer_show;                     /* ticks the pointer stays shown after the C-stick (loader-filled) */
    int32_t pointer[RTGCWII_CHANNELS][2];      /* each channel's pointer, x and y over RTHOME_POINTER_FULL */
    uint32_t pointer_moved[RTGCWII_CHANNELS];  /* when the C-stick last moved it (low bit set), 0: never */
    uint32_t shake[RTGCWII_CHANNELS];          /* samples the shake button has been held, 0: not held */
    uint32_t camera;  /* channels whose Wii Remote camera the game wants on (KPAD's switch), one bit each */
    /* Where KPAD keeps each channel's camera switch (KPADEnableDPD's
     * code): the byte at kpad_dpd + channel * kpad_dpd_stride +
     * kpad_dpd_offset, 0: not found (the pointer shows only after the
     * C-stick moves). Loader-filled. */
    uint32_t kpad_dpd, kpad_dpd_stride, kpad_dpd_offset;
    /* The HOME Menu's stand-in's channel (one bit) when the game knew it
     * as this stand-in's as the HOME stand-in took it, 0: a Classic one.
     * It keeps that shape until the next time it takes a channel, though
     * the controller unplugs under the menu (the Wii Remote forms only). */
    uint32_t home_shaped;
    /* A 2006 KPAD game's WPADGetInfoAsync callback, answered for the
     * stand-in and told at the game's next KPAD read on that channel (as a
     * real Wii Remote's answer comes later), 0: none waiting. */
    uint32_t info_callback[RTGCWII_CHANNELS];
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
    uint32_t retell_ticks;
    uint32_t heard;                         /* channels read since they were told, one bit each */
    uint32_t told_at[RTGCWII_CHANNELS];     /* when each was told (low bit set), 0: not noted yet */
    uint32_t retells[RTGCWII_CHANNELS];     /* times told again since its controller came */
    /* wpad_connect_table is the later WPAD's (find_wpad_connect_later): the
     * game's sleeps don't read the pads for the stand-in (pad_hook.c's
     * pad_reads_at_sleep). Loader-filled. */
    uint32_t wpad_connect_later;
    /* WPAD's extension callbacks (the game's, WPADSetExtensionCallback):
     * the table of the channels' control blocks and the callback's place in
     * one (just before the connect callback's), 0: none. Told the stand-in's
     * attachment after its "connected" (pad_tell_extension). Loader-filled. */
    uint32_t wpad_ext_table, wpad_ext_offset;
    /* "Connected only" (a pack does the controls: it reads the GameCube
     * ports itself): the stand-in's channel is connected and answered for
     * as usual, but its input stays at rest (rtgcwii_pads), a read that
     * brings samples (the pack's) is left to the game, and nothing counts
     * as a real Wii Remote taking the channel back. Loader-filled. */
    uint32_t inputless;
};

#define RTGCWII_RETELLS 30u

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

/* Whether the stand-in answers on `channel` now. */
int rtgcwii_on(const struct rtgcwii* w, uint32_t channel);

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

/* A Wii Remote + Nunchuk as the game's KPAD read takes it: the words, the
 * Nunchuk's stick, both accelerometers (still, or shaking while X is held),
 * and the pointer, all numerators over RTHOME_AXIS_FULL but the pointer's
 * (over RTHOME_POINTER_FULL). */
struct rtgcwii_fs {
    uint32_t hold, trig, release;
    int32_t stick[2];
    int32_t acc[3], acc_value, acc_speed;           /* the Wii Remote's */
    int32_t fs_acc[3], fs_acc_value, fs_acc_speed;  /* the Nunchuk's */
    int32_t pointer[2];
    int pointer_shown;  /* the game has the camera on, or the C-stick moved it within pointer_show ticks */
};

/* The shake: all three axes swing between +RTGCWII_SHAKE_G and -SHAKE_G,
 * flipping every RTGCWII_SHAKE_FLIP samples while X is held, as Dolphin's
 * emulated shake does; 3.4 g is past the threshold Batman: The Brave and
 * the Bold checks for its heavy attacks (3.2 g for the Wii Remote, 2.0 g
 * for the Nunchuk). Numerators over RTHOME_AXIS_FULL (256: 1 g). */
#define RTGCWII_SHAKE_G 870
#define RTGCWII_SHAKE_VALUE 1508  /* |(3.4, 3.4, 3.4)| g */
#define RTGCWII_SHAKE_FLIP 4u
/* Full C-stick moves the pointer this much a sample (the screen is two
 * RTHOME_POINTER_FULL wide, so about a second across), and it stays inside
 * +-RTGCWII_POINTER_EDGE, so the cursor never leaves the screen. */
#define RTGCWII_POINTER_STEP 143
#define RTGCWII_POINTER_EDGE 3891
/* The sideways form's tilt: the C-stick held right turns the Wii Remote as
 * a wheel steering right, gravity from -Y towards -Z (Dolphin's emulated
 * sideways Wii Remote, Excite Truck's tutorial, 22 Nov): full C-stick is
 * sin 55 degrees of it, over RTHOME_AXIS_FULL. */
#define RTGCWII_TILT_SIN 210

/* The game wants `channel`'s Wii Remote camera on (`mode` not 0) or off,
 * as KPAD's switch says (KPADEnableDPD, KPADDisableDPD): a game turns it on
 * for its pointer menus (Batman: The Brave and the Bold from its title
 * screen on), so the stand-in's pointer is shown while it is on, as well as
 * after the C-stick moves. */
void rtgcwii_camera(struct rtgcwii* w, uint32_t channel, uint32_t mode);

/* A numerator over RTHOME_AXIS_FULL as a float's bits, as rthome_float_bits
 * but with no cap at 1: the Wii Remote's shake swings past 3 g, and the
 * pointer's distance at rest is about 1.95 (the form's KPAD status). */
uint32_t rtgcwii_float_bits(int32_t numerator);

/* The stand-in's Wii Remote + Nunchuk on `channel` (RT_PAD_FORM_NUNCHUK),
 * for the game's KPAD read at `now`: the same words for every read until
 * the next sample, as rtgcwii_classic gives. All 0 (a still controller,
 * pointer hidden) where the stand-in isn't on. */
void rtgcwii_nunchuk(struct rtgcwii* w, uint32_t channel, uint32_t now, struct rtgcwii_fs* out);

/* The stand-in's sideways Wii Remote on `channel` (RT_PAD_FORM_SIDEWAYS),
 * for the game's KPAD read: the words, the acceleration (tilted by the
 * C-stick's left and right while the game has the camera off, shaking while
 * R is held) and, while the game has the camera on, the pointer the C-stick
 * moves. The Nunchuk's fields stay at rest. All 0 where the stand-in isn't
 * on. */
void rtgcwii_sideways(struct rtgcwii* w, uint32_t channel, struct rtgcwii_fs* out);

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
#define RTGCWII_NO_ERROR 0x7FFFFFFF
#define RTGCWII_EMPTY_READS 30
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

/* At each PADRead: a channel the game was told of and hasn't read since,
 * retell_ticks after the telling, counts as not told, so the connect
 * callback hears "connected" again (rtgcwii_next_change), at most
 * RTGCWII_RETELLS times while its controller stays. */
void rtgcwii_retell(struct rtgcwii* w, uint32_t now);

/* WPADStatus, as WPADRead gives it: the buttons (big-endian) at 0, the
 * acceleration at 2, 4 and 6 (signed, RTGCWII_ACC_G a g, the Wii Remote's
 * own axes: lying flat, +z is up), four camera objects at 8 (x and y signed
 * on 1024 x 768, the size, an id and a pad byte; size 0: none), the device
 * type at 40 and the error at 41; a Wii Remote + Nunchuk's WPADFSStatus goes
 * on with the Nunchuk's acceleration at 42, 44 and 46 and its stick at 48
 * and 49 (signed bytes, RTGCWII_FS_STICK_FULL full). */
#define RTGCWII_WPAD_CORE_BYTES 42u
#define RTGCWII_WPAD_FS_BYTES 50u
#define RTGCWII_ACC_G 104         /* Dolphin's calibration: zero 0x200, one g 0x268 */
#define RTGCWII_FS_STICK_FULL 100
/* The camera's two sensor-bar dots: centred on (512, 384) when the pointer
 * is at the screen's centre, moving against the pointer by up to
 * RTGCWII_DOT_RANGE_X / Y at its edge, RTGCWII_DOT_APART apart (Dolphin's
 * emulated Wii Remote at rest: 137 pixels, size 3). */
#define RTGCWII_DOT_RANGE_X 384
#define RTGCWII_DOT_RANGE_Y 288
#define RTGCWII_DOT_APART 137
#define RTGCWII_DOT_SIZE 3u

/* The stand-in's raw status on `channel` (the Wii Remote forms) into `out`,
 * for a game that reads WPAD itself: its form's buttons and acceleration
 * (rtgcwii_sideways, rtgcwii_nunchuk) and, while the camera is on (KPAD's
 * switch, `kpad_camera`, or a data format with the camera that the game
 * asked for), the pointer as the two dots; the Nunchuk's part only when the
 * game asked for a Nunchuk format (WPADRead copies that format's size).
 * Returns the bytes written, 0 (none) where the stand-in isn't on. In the
 * Classic Controller form it writes nothing. */
uint32_t rtgcwii_wpad_status(struct rtgcwii* w, uint32_t channel, uint8_t* out, uint32_t now, int kpad_camera);

#endif
