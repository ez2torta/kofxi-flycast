// KOF XI training cheats for Flycast: a port of recomp/devices/kofxi_cheats.cpp
// (kofxi_aw_modding). Same tokens, same guest addresses, same per-vblank writes.
//
// Not part of the original game. Everything is off unless KOFXI_CHEATS (tokens) or
// KOFXI_CHEATS_FILE (a text file with the tokens, reloaded when it changes) is set.
// Not ported yet: rematch (A+Start / B+Start) and the restart request.
#include "kofxi_cheats.h"
#include <string>
#include "hw/sh4/sh4_mem.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
constexpr u32 kJudgementScore = 0x8C27D392u;  // s16, +P1 / -P2, clamp +-90
constexpr u32 kRoundClock = 0x8C27CB78u;       // u16 seconds, 99 at round start
constexpr u32 kVsLimitCode = 0x8C369E13u;
constexpr u32 kStageId = 0x8C2179A0u;
constexpr u32 kUnlockMask = 0x8C18F09Au;
constexpr u32 kTeam0 = 0x8C27CB54u;
constexpr u32 kTeamStride = 0x1F8u;
constexpr u32 kDbgFlags0 = 0x8C35AB38u;  // Time Stop 0x10, Seq Stop 0x8000
constexpr u32 kDbgFlags1 = 0x8C35AB3Cu;  // L CMD FREE 0x20000000
constexpr u32 kDbgFlags2 = 0x8C35AB40u;  // PS GAI..MAGAKI 0x01..0x40, PS SAME 0x80
constexpr u32 kDbgTimeStop = 0x10u, kDbgSeqStop = 0x8000u, kDbgLdmAny = 0x20000000u;
constexpr u32 kDbgPsAll = 0x7Fu, kDbgPsSame = 0x80u;
constexpr u16 kMaxHealth = 112;

using Options = KofxiCheats;

Options g_opts;
Options& opts = g_opts;
u32 owned[3] = {};
u8 lifePrev[2] = {};
int dmgPoint = -1;
u16 dmgHealth = 0;
bool dmgArmed = false;
std::string filePath;
std::filesystem::file_time_type fileMtime{};
bool fileSeen = false;
u64 frameNo = 0;
bool initDone = false;

// Guest address (0x8Cxxxxxx cached mirror of RAM) to the RAM byte.
inline u8* ram(u32 addr) { return &mem_b[addr & 0x00FFFFFFu]; }
inline u8 rd8(u32 a) { return *ram(a); }
inline u16 rd16(u32 a) { u16 v; std::memcpy(&v, ram(a), 2); return v; }
inline u32 rd32(u32 a) { u32 v; std::memcpy(&v, ram(a), 4); return v; }
inline void wr8(u32 a, u8 v) { *ram(a) = v; }
inline void wr16(u32 a, u16 v) { std::memcpy(ram(a), &v, 2); }
inline void wr32(u32 a, u32 v) { std::memcpy(ram(a), &v, 4); }

bool allDigits(const std::string& s, size_t from)
{
	return s.size() > from && s.find_first_not_of("0123456789", from) == std::string::npos;
}

void parse(Options& o, std::string spec)
{
	for (size_t h = spec.find('#'); h != std::string::npos; h = spec.find('#', h))
	{
		size_t eol = spec.find('\n', h);
		spec.erase(h, eol == std::string::npos ? std::string::npos : eol - h);
	}
	for (char& c : spec)
		if (c == ';' || c == '\n' || c == '\r' || c == '\t')
			c = ',';
	size_t pos = 0;
	while (pos <= spec.size())
	{
		size_t end = spec.find(',', pos);
		std::string it = spec.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
		pos = end == std::string::npos ? spec.size() + 1 : end + 1;
		while (!it.empty() && it.front() == ' ') it.erase(it.begin());
		while (!it.empty() && it.back() == ' ') it.pop_back();
		if (it.empty() || it == "none")
			continue;
		if (it == "health1") o.health[0] = true;
		else if (it == "health2") o.health[1] = true;
		else if (it == "super1") o.super[0] = true;
		else if (it == "super2") o.super[1] = true;
		else if (it == "tag1") o.tag[0] = true;
		else if (it == "tag2") o.tag[1] = true;
		else if (it == "ko1") o.p1_ko = true;
		else if (it == "ko2") o.p2_ko = true;
		else if (it == "ko") o.p1_ko = o.p2_ko = true;
		else if (it == "damage1") o.damage1 = 8;
		else if (it.compare(0, 8, "damage1x") == 0 && allDigits(it, 8))
		{
			unsigned m = (unsigned)std::strtoul(it.c_str() + 8, nullptr, 10);
			if (m >= 2 && m <= 32) o.damage1 = m;
		}
		else if (it == "unlock") o.unlock = true;
		else if (it == "judge1") o.judge = 1;
		else if (it == "draw") o.judge = 2;
		else if (it == "p2win") o.judge = 3;
		else if (it == "same") o.same = true;
		else if (it == "timestop") o.timestop = true;
		else if (it == "ldm") o.ldm = true;
		else if (it == "cpustop") o.cpustop = true;
		else if (it == "vsmode") o.vs_limit = 5;
		else if (it.size() == 8 && it.compare(0, 7, "vslimit") == 0 && it[7] >= '0' && it[7] <= '3')
			o.vs_limit = (unsigned)(it[7] - '0') + 1;
		else if (it.size() == 6 && it.compare(0, 5, "stage") == 0 && it[5] >= '0' && it[5] <= '8')
			o.stage = (unsigned)(it[5] - '0') + 1;
		else if (it.size() > 5 && it.compare(0, 3, "dbg") == 0 && it[3] >= '0' && it[3] <= '2' && it[4] == '=')
			o.dbg[it[3] - '0'] = (u32)std::strtoul(it.c_str() + 5, nullptr, 0);
		else if (it.compare(0, 4, "time") == 0 && allDigits(it, 4))
		{
			unsigned s = (unsigned)std::strtoul(it.c_str() + 4, nullptr, 10);
			if (s >= 1 && s <= 98) o.round_time = s;
		}
		else if (it == "all")
		{
			o.health[0] = o.health[1] = o.super[0] = o.super[1] = o.tag[0] = o.tag[1] = true;
		}
		else
			std::fprintf(stderr, "kofxi cheats: unknown token '%s'\n", it.c_str());
	}
}

void pollFile()
{
	if (filePath.empty() || (frameNo % 20) != 0)
		return;
	std::error_code ec;
	auto mtime = std::filesystem::last_write_time(filePath, ec);
	if (ec || (fileSeen && mtime == fileMtime))
		return;
	fileSeen = true;
	fileMtime = mtime;
	std::ifstream in(filePath);
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	Options next;
	parse(next, text);
	if (next != opts)
	{
		opts = next;
		std::fprintf(stderr, "kofxi cheats: reloaded from %s (frame %llu)\n", filePath.c_str(),
				(unsigned long long)frameNo);
	}
}

// Multiplies only observed health losses on P2's active point (same rule as the recomp).
u16 damageBoost(u16 previous, u16 current, unsigned m)
{
	if (m <= 1 || current >= previous || current > kMaxHealth || previous > kMaxHealth)
		return current;
	u64 extra = (u64)(previous - current) * (m - 1);
	return extra >= current ? 0 : (u16)(current - extra);
}

void apply()
{
	// Debug-menu flags: only the bits this module owns are touched and cleared.
	u32 want0 = (opts.timestop ? kDbgTimeStop : 0) | (opts.cpustop ? kDbgSeqStop : 0) | opts.dbg[0];
	u32 want1 = (opts.ldm ? kDbgLdmAny : 0) | opts.dbg[1];
	u32 want2 = (opts.unlock ? kDbgPsAll : 0) | (opts.same ? kDbgPsSame : 0) | opts.dbg[2];
	const u32 addrs[3] = {kDbgFlags0, kDbgFlags1, kDbgFlags2};
	const u32 wants[3] = {want0, want1, want2};
	for (int i = 0; i < 3; ++i)
	{
		u32 word = rd32(addrs[i]);
		u32 next = (word & ~owned[i]) | wants[i];
		if (next != word)
			wr32(addrs[i], next);
		owned[i] = wants[i];
	}
	if (!opts.active() && lifePrev[0] == 0 && lifePrev[1] == 0 && !dmgArmed)
		return;

	if (opts.unlock)
		wr8(kUnlockMask, 0xFF);
	if (opts.judge != 0)
		wr16(kJudgementScore, (u16)(s16)(opts.judge == 1 ? 90 : opts.judge == 3 ? -90 : 0));
	if (opts.vs_limit != 0)
		wr8(kVsLimitCode, (u8)(opts.vs_limit - 1));
	if (opts.stage != 0)
		wr8(kStageId, (u8)(opts.stage - 1));
	if (opts.round_time != 0)
	{
		s16 clock = (s16)rd16(kRoundClock);
		if (clock > (s16)opts.round_time && clock <= 99)
			wr16(kRoundClock, (u16)opts.round_time);
	}
	for (unsigned side = 0; side < 2; ++side)
	{
		u32 team = kTeam0 + side * kTeamStride;
		u8 point = rd8(team + 0x53);
		u8 life = (side == 0 ? opts.p1_ko : opts.p2_ko) ? 2 : opts.health[side] ? 1 : 0;
		if (point < 3)
		{
			u32 a = team + 0x150 + point * 0x20 + 4;
			if (life == 1) wr16(a, kMaxHealth);
			else if (life == 2) wr16(a, 1);
			else if (lifePrev[side] != 0) wr16(a, kMaxHealth);
		}
		lifePrev[side] = life;
		if (opts.super[side]) wr32(team + 0x88, 0xE0);
		if (opts.tag[side]) wr32(team + 0x8C, 0xE0);
	}
	if (opts.damage1 > 1)
	{
		u32 team = kTeam0 + kTeamStride;
		u8 point = rd8(team + 0x53);
		if (point >= 3)
		{
			dmgPoint = -1;
			dmgArmed = false;
		}
		else
		{
			u32 a = team + 0x150 + point * 0x20 + 4;
			u16 cur = rd16(a);
			if (cur > kMaxHealth)
			{
				dmgPoint = -1;
				dmgArmed = false;
			}
			else if (!dmgArmed || dmgPoint != point)
			{
				dmgPoint = point;
				dmgHealth = cur;
				dmgArmed = true;
			}
			else
			{
				u16 boosted = damageBoost(dmgHealth, cur, opts.damage1);
				if (boosted != cur)
					wr16(a, boosted);
				dmgHealth = boosted;
			}
		}
	}
	else
	{
		dmgPoint = -1;
		dmgArmed = false;
	}
}
}	// namespace

// Called once per vblank from the SPG, after the guest has run the frame.
void kofxi_cheats_vblank()
{
	if (!initDone)
	{
		initDone = true;
		if (const char* f = std::getenv("KOFXI_CHEATS_FILE"); f && *f)
		{
			filePath = f;
			std::fprintf(stderr, "kofxi cheats: watching %s\n", f);
		}
		if (const char* e = std::getenv("KOFXI_CHEATS"); e && *e)
			parse(opts, e);
		if (filePath.empty() && !opts.active())
			filePath = "kofxi_cheats.txt";	// optional file next to the executable's working dir
	}
	++frameNo;
	pollFile();
	if (!opts.active() && owned[0] == 0 && owned[1] == 0 && owned[2] == 0 && lifePrev[0] == 0
			&& lifePrev[1] == 0 && !dmgArmed)
		return;
	apply();
}

KofxiCheats& kofxiCheats() { return g_opts; }

std::string kofxiCheatsToText()
{
	const Options& o = g_opts;
	std::string t;
	auto add = [&](const std::string& x) { t += (t.empty() ? "" : ",") + x; };
	for (int i = 0; i < 2; ++i)
	{
		std::string n = std::to_string(i + 1);
		if (o.health[i]) add("health" + n);
		if (o.super[i]) add("super" + n);
		if (o.tag[i]) add("tag" + n);
	}
	if (o.p1_ko && o.p2_ko) add("ko");
	else { if (o.p1_ko) add("ko1"); if (o.p2_ko) add("ko2"); }
	if (o.damage1 > 1) add("damage1x" + std::to_string(o.damage1));
	if (o.unlock) add("unlock");
	if (o.same) add("same");
	if (o.timestop) add("timestop");
	if (o.ldm) add("ldm");
	if (o.cpustop) add("cpustop");
	if (o.judge == 1) add("judge1"); else if (o.judge == 2) add("draw"); else if (o.judge == 3) add("p2win");
	if (o.round_time) add("time" + std::to_string(o.round_time));
	if (o.stage) add("stage" + std::to_string(o.stage - 1));
	if (o.vs_limit == 5) add("vsmode"); else if (o.vs_limit) add("vslimit" + std::to_string(o.vs_limit - 1));
	return t;
}
