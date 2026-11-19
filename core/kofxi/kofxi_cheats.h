// KOF XI training cheats (port of recomp/devices/kofxi_cheats.*). See kofxi_cheats.cpp.
#pragma once
#include "types.h"
#include <tuple>

struct KofxiCheats
{
	bool health[2] {}, super[2] {}, tag[2] {};
	bool p1_ko = false, p2_ko = false, unlock = false, same = false, timestop = false;
	bool ldm = false, cpustop = false;
	unsigned judge = 0;       // 0 off, 1 P1 wins on time, 2 draw, 3 P2 wins on time
	unsigned round_time = 0;  // 0 off, 1..98 seconds
	unsigned stage = 0;       // 0 auto, 1..9 = stage 0..8
	unsigned vs_limit = 0;    // 0 game settings, 1..4 = limit code 0..3, 5 = event mode
	unsigned damage1 = 0;     // 0 off, 2..32 multiplier on P2 health losses
	u32 dbg[3] {};

	auto tie() const
	{
		return std::tie(health[0], health[1], super[0], super[1], tag[0], tag[1], p1_ko, p2_ko, unlock,
				same, timestop, ldm, cpustop, judge, round_time, stage, vs_limit, damage1, dbg[0], dbg[1], dbg[2]);
	}
	bool operator==(const KofxiCheats& o) const { return tie() == o.tie(); }
	bool operator!=(const KofxiCheats& o) const { return !(*this == o); }
	bool active() const { return *this != KofxiCheats{}; }
};

// Live options, edited by the in-game menu and by KOFXI_CHEATS / KOFXI_CHEATS_FILE.
KofxiCheats& kofxiCheats();
// Writes the current options as tokens (same syntax as KOFXI_CHEATS).
std::string kofxiCheatsToText();
