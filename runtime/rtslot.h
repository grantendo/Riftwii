/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Slot ownership, the third of the stand-in's four layers (sources, shapes,
 * slot ownership, delivery; notes in #587): for each Wii Remote slot (WPAD's
 * "channel"), whether a real controller, the stand-in or nobody holds it, and
 * what the game's connect callback has been told. The hooks report what they
 * saw; only this module decides. Free of the hardware, so the host tests run
 * it. Step 1 of the restructure: the decisions rtgcwii made, moved here as
 * they were. */
#ifndef RIFTWII_RTSLOT_H
#define RIFTWII_RTSLOT_H

#include <stdint.h>

#define RTSLOT_SLOTS 4u
#define RTSLOT_RETELLS 30u
#define RTSLOT_NO_ERROR 0x7FFFFFFF
#define RTSLOT_EMPTY_READS 30

struct rtslot {
    uint32_t grace;    /* ticks a port may report no controller before its channel's goes (loader-filled) */
    uint32_t present;  /* channels with a GameCube controller behind them, one bit each */
    uint32_t real;     /* channels where WPADProbe last found a real controller: the stand-in steps aside */
    uint32_t told;     /* channels the game's connect callback last heard are connected (by this) */
    uint32_t telling;    /* how many tellings of the game's connect callback are under way (PADReads meanwhile leave it to them) */
    uint32_t empty_since[RTSLOT_SLOTS];
    uint32_t empty_reads[RTSLOT_SLOTS];
    uint32_t empty_last[RTSLOT_SLOTS];  /* the last empty read (low bit set): a gap past `grace` starts over */
    uint32_t real_checked;
    uint32_t home_shaped;
    uint32_t retell_ticks;
    uint32_t heard;                         /* channels read since they were told, one bit each */
    uint32_t told_at[RTSLOT_SLOTS];     /* when each was told (low bit set), 0: not noted yet */
    uint32_t retells[RTSLOT_SLOTS];     /* times told again since its controller came */
    uint32_t retell;
    uint32_t wii_read, quiet_from;
    uint32_t quiet_reached;
    uint32_t probe_counts[RTSLOT_SLOTS];  /* rtslot_probe_count */
};

uint32_t rtslot_probe_count(const struct rtslot* s, uint32_t slot);
void rtslot_probed(struct rtslot* s, uint32_t slot, int32_t result);
int rtslot_on(const struct rtslot* s, uint32_t slot);
uint32_t rtslot_shaped(const struct rtslot* s);
void rtslot_home_took(struct rtslot* s, uint32_t slot);
int rtslot_home_keeps_shape(const struct rtslot* s, uint32_t slot);
void rtslot_connect_change(struct rtslot* s, uint32_t* leave, uint32_t* join);
void rtslot_wii_read(struct rtslot* s, uint32_t now);
int rtslot_quiet_due(struct rtslot* s, uint32_t now, uint32_t ticks_per_ms);
int rtslot_real_check_due(struct rtslot* s, uint32_t now, uint32_t ticks_per_ms);
int rtslot_next_change(struct rtslot* s, uint32_t skip, uint32_t keep, uint32_t* slot, int32_t* reason);
void rtslot_kpad_read(struct rtslot* s, uint32_t slot, int32_t result, int32_t error, uint32_t now);
void rtslot_still_told(struct rtslot* s, uint32_t slot);
void rtslot_not_told(struct rtslot* s, uint32_t slot);
void rtslot_heard(struct rtslot* s, uint32_t slot);
void rtslot_retell(struct rtslot* s, uint32_t now);

#endif
