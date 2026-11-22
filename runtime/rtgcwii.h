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
};

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

#endif
