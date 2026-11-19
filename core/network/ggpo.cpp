/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "ggpo.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "input/mouse.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "oslib/i18n.h"
#include "net_platform.h"
#include <algorithm>

void kofxiBenchReset(unsigned mask);
extern bool *unprotected_pages;
namespace ggpo
{

bool inRollback;

static void getLocalInput(MapleInputState inputState[4])
{
	if (!config::ThreadedRendering)
		os_UpdateInputState();
	std::lock_guard<std::mutex> lock(relPosMutex);
	for (int player = 0; player < 4; player++)
	{
		MapleInputState& state = inputState[player];
		state.kcode = kcode[player];
		state.halfAxes[PJTI_L] = lt[player];
		state.halfAxes[PJTI_R] = rt[player];
		state.halfAxes[PJTI_L2] = lt2[player];
		state.halfAxes[PJTI_R2] = rt2[player];
		state.fullAxes[PJAI_X1] = joyx[player];
		state.fullAxes[PJAI_Y1] = joyy[player];
		state.fullAxes[PJAI_X2] = joyrx[player];
		state.fullAxes[PJAI_Y2] = joyry[player];
		state.fullAxes[PJAI_X3] = joy3x[player];
		state.fullAxes[PJAI_Y3] = joy3y[player];
		state.mouseButtons = mo_buttons[player];
		state.absPos.x = mo_x_abs[player];
		state.absPos.y = mo_y_abs[player];
		state.keyboard.shift = kb_shift[player];
		memcpy(state.keyboard.key, kb_key[player], sizeof(kb_key[player]));
		int relX = std::round(mo_x_delta[player]);
		int relY = std::round(mo_y_delta[player]);
		int wheel = std::round(mo_wheel_delta[player]);
		state.relPos.x += relX;
		state.relPos.y += relY;
		state.relPos.wheel += wheel;
		mo_x_delta[player] -= relX;
		mo_y_delta[player] -= relY;
		mo_wheel_delta[player] -= wheel;
	}
}

}

#ifdef USE_GGPO
#include "ggponet.h"
#include "emulator.h"
#include "ui/gui.h"
#include "ui/gui_util.h"
#include "hw/mem/mem_watch.h"
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <numeric>
#include "imgui.h"
#include "miniupnp.h"
#include "hw/naomi/naomi_cart.h"

//#define SYNC_TEST 1

#include <xxhash.h>
#include <deque>
#include <fstream>
#include <array>
#include <cmath>
#include "input/gamepad_device.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/aica/aica_if.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/dyna/ngen.h"

namespace ggpo
{
using namespace std::chrono;

constexpr int MAX_PLAYERS = 2;

// KOFXI rollback bench (temporary): time spent in save / load / resimulated frame.
struct BenchStat {
	double ms = 0, maxMs = 0; u64 n = 0;
	void add(double v) { ms += v; if (v > maxMs) maxMs = v; ++n; }
};
static BenchStat benchSave, benchLoad, benchAdvance;
static u64 benchBytes = 0, benchPagesRam = 0, benchPagesVram = 0, benchPagesAram = 0;
static double benchNow() {
	static const auto base = std::chrono::steady_clock::now();
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - base).count();
}
static bool benchOn() {
	static const bool on = std::getenv("KOFXI_ROLLBACK_BENCH") != nullptr;
	return on;
}

static void benchReport() {
	static FILE* f = nullptr;
	if (f == nullptr) {
		const char* path = std::getenv("KOFXI_ROLLBACK_LOG");
		f = std::fopen(path ? path : "rollback_bench.txt", "w");
	}
	if (f == nullptr || benchSave.n == 0) return;
	std::fprintf(f, "saves %llu save_avg %.3f ms save_max %.3f | loads %llu load_avg %.3f ms load_max %.3f | "
			"resim %llu advance_avg %.3f ms max %.3f | state_bytes_avg %.0f | pages/frame ram %.1f vram %.1f aram %.1f\n",
			(unsigned long long)benchSave.n, benchSave.ms / benchSave.n, benchSave.maxMs,
			(unsigned long long)benchLoad.n, benchLoad.n ? benchLoad.ms / benchLoad.n : 0.0, benchLoad.maxMs,
			(unsigned long long)benchAdvance.n, benchAdvance.n ? benchAdvance.ms / benchAdvance.n : 0.0, benchAdvance.maxMs,
			(double)benchBytes / benchSave.n, (double)benchPagesRam / benchSave.n,
			(double)benchPagesVram / benchSave.n, (double)benchPagesAram / benchSave.n);
	std::fflush(f);
}
constexpr int SERVER_PORT = 19713;

constexpr u32 BTN_TRIGGER_LEFT	= DC_BTN_BITMAPPED_LAST << 1;
constexpr u32 BTN_TRIGGER_RIGHT	= DC_BTN_BITMAPPED_LAST << 2;

#pragma pack(push, 1)
struct VerificationData
{
	const int protocol = 2;
	u8 gameMD5[16] { };
	u8 stateMD5[16] { };
} ;
#pragma pack(pop)

static GGPOSession *ggpoSession;
static int localPlayerNum;
static GGPOPlayerHandle localPlayer;
static GGPOPlayerHandle remotePlayer;
static bool synchronized;
static std::recursive_mutex ggpoMutex;
static std::array<int, 5> msPerFrame;
static int msPerFrameIndex;
static time_point<steady_clock> lastFrameTime;
static int msPerFrameAvg;
static bool _endOfFrame;
static MiniUPnP miniupnp;
static int analogAxes;
static bool absPointerPos;
static bool keyboardGame;
static bool mouseGame;
static int inputSize;
static void (*chatCallback)(int playerNum, const std::string& msg);

struct MemPages
{
	void load()
	{
		memwatch::ramWatcher.getPages(ram);
		memwatch::vramWatcher.getPages(vram);
		memwatch::aramWatcher.getPages(aram);
		memwatch::elanWatcher.getPages(elanram);
	}
	memwatch::PageMap ram;
	memwatch::PageMap vram;
	memwatch::PageMap aram;
	memwatch::PageMap elanram;
};
static std::unordered_map<int, MemPages> deltaStates;
static int lastSavedFrame = -1;

static int timesyncOccurred;

#pragma pack(push, 1)
struct Inputs
{
	u32 kcode:20;
	u32 mouseButtons:4;
	u32 kbModifiers:8;

	union {
		struct {
			u8 x;
			u8 y;
		} analog;
		struct {
			s16 x;
			s16 y;
		} absPos;
		struct {
			s16 x;
			s16 y;
			s16 wheel;
		} relPos;
		u8 keys[6];
	} u;
};
static_assert(sizeof(Inputs) == 10, "wrong Inputs size");
static_assert(BTN_TRIGGER_RIGHT < (1 << 20));

struct GameEvent
{
	enum : char {
		Chat,
		VF4Card
	} type;
	union {
		struct {
			u8 playerNum;
			char message[512 - sizeof(playerNum) - sizeof(type)];
		} chat;
		struct {
			u8 playerNum;
			u8 data[128];
		} card;
	} u;

	constexpr static int chatMessageLen(int len) { return len - sizeof(u.chat.playerNum) - sizeof(type); }
};
#pragma pack(pop)

/*
 * begin_game callback - This callback has been deprecated.  You must
 * implement it, but should ignore the 'game' parameter.
 */
static bool begin_game(const char *)
{
	DEBUG_LOG(NETWORK, "Game begin");
	rend_allow_rollback();
	return true;
}

/*
 * on_event - Notification that something has happened.  See the GGPOEventCode
 * structure for more information.
 */
static bool on_event(GGPOEvent *info)
{
	using namespace i18n;
	switch (info->code) {
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		INFO_LOG(NETWORK, "Connected to peer %d", info->u.connected.player);
		os_notify(T("Connected to peer"), 2000);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		INFO_LOG(NETWORK, "Synchronizing with peer %d", info->u.synchronizing.player);
		os_notify(T("Synchronizing with peer"), 2000);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
		INFO_LOG(NETWORK, "Synchronized with peer %d", info->u.synchronized.player);
		os_notify(T("Synchronized with peer"), 2000);
		break;
	case GGPO_EVENTCODE_RUNNING:
		INFO_LOG(NETWORK, "Running");
		os_notify(T("Running"), 2000);
		synchronized = true;
		break;
	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		INFO_LOG(NETWORK, "Disconnected from peer %d", info->u.disconnected.player);
		throw FlycastException(Ts("Disconnected from peer"));
		break;
	case GGPO_EVENTCODE_TIMESYNC:
		INFO_LOG(NETWORK, "Timesync: %d frames ahead", info->u.timesync.frames_ahead);
		timesyncOccurred += 5;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (msPerFrameAvg >= 25 ? 30 : 60)));
		break;
	case GGPO_EVENTCODE_CONNECTION_INTERRUPTED:
		INFO_LOG(NETWORK, "Connection interrupted with player %d", info->u.connection_interrupted.player);
		os_notify(T("Connection interrupted"), 2000);
		break;
	case GGPO_EVENTCODE_CONNECTION_RESUMED:
		INFO_LOG(NETWORK, "Connection resumed with player %d", info->u.connection_resumed.player);
		os_notify(T("Connection resumed"), 2000);
		break;
	}
	return true;
}

/*
 * advance_frame - Called during a rollback.  You should advance your game
 * state by exactly one frame.  Before each frame, call ggpo_synchronize_input
 * to retrieve the inputs you should use for that frame.  After each frame,
 * you should call ggpo_advance_frame to notify GGPO.net that you're
 * finished.
 *
 * The flags parameter is reserved.  It can safely be ignored at this time.
 */
static bool advance_frame(int)
{
	INFO_LOG(NETWORK, "advance_frame");
	settings.aica.muteAudio = true;
	rend_enable_renderer(false);
	inRollback = true;

	double t0 = benchNow();
	emu.run();
	benchAdvance.add(benchNow() - t0);
	ggpo_advance_frame(ggpoSession);

	settings.aica.muteAudio = false;
	rend_enable_renderer(true);
	inRollback = false;
	_endOfFrame = false;

	return true;
}

/*
 * load_game_state - GGPO.net will call this function at the beginning
 * of a rollback.  The buffer and len parameters contain a previously
 * saved state returned from the save_game_state function.  The client
 * should make the current game state match the state contained in the
 * buffer.
 */
static bool load_game_state_impl(unsigned char *buffer, int len);
static bool load_game_state(unsigned char *buffer, int len)
{
	double t0 = benchNow();
	bool r = load_game_state_impl(buffer, len);
	benchLoad.add(benchNow() - t0);
	return r;
}
static bool load_game_state_impl(unsigned char *buffer, int len)
{
	INFO_LOG(NETWORK, "load_game_state");

	rend_start_rollback();
	// FIXME dynarecs
	Deserializer deser(buffer, len, true);
	int frame;
	deser >> frame;
	memwatch::unprotect();
	for (int f = lastSavedFrame - 1; f >= frame; f--)
	{
		const MemPages& pages = deltaStates[f];
		for (const auto& pair : pages.ram)
			memcpy(memwatch::ramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.vram)
			memcpy(memwatch::vramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.aram)
			memcpy(memwatch::aramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.elanram)
			memcpy(memwatch::elanWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		DEBUG_LOG(NETWORK, "Restored frame %d pages: %d ram, %d vram, %d eram, %d aica ram", f, (u32)pages.ram.size(),
					(u32)pages.vram.size(), (u32)pages.elanram.size(), (u32)pages.aram.size());
	}
	dc_deserialize(deser);
	if (deser.size() != (u32)len)
	{
		ERROR_LOG(NETWORK, "load_game_state len %d used %d", len, (int)deser.size());
		die("fatal");
	}
	rend_allow_rollback();	// ggpo might load another state right after this one
	memwatch::reset();
	memwatch::protect();
	return true;
}

/*
 * save_game_state - The client should allocate a buffer, copy the
 * entire contents of the current game state into it, and copy the
 * length into the *len parameter.  Optionally, the client can compute
 * a checksum of the data and store it in the *checksum argument.
 */
static bool save_game_state_impl(unsigned char **buffer, int *len, int *checksum, int frame);
static bool save_game_state(unsigned char **buffer, int *len, int *checksum, int frame)
{
	double t0 = benchNow();
	bool r = save_game_state_impl(buffer, len, checksum, frame);
	benchSave.add(benchNow() - t0);
	if (r) {
		benchBytes += *len;
		if (frame > 0 && deltaStates.count(frame - 1)) {
			const MemPages& d = deltaStates[frame - 1];
			benchPagesRam += d.ram.size(); benchPagesVram += d.vram.size(); benchPagesAram += d.aram.size();
		}
	}
	if (benchSave.n % 300 == 0) benchReport();
	return r;
}
static bool save_game_state_impl(unsigned char **buffer, int *len, int *checksum, int frame)
{
	verify(!emu.getSh4Executor()->IsCpuRunning());
	lastSavedFrame = frame;
	// TODO this is way too much memory
	size_t allocSize = settings.platform.isNaomi() ? 20_MB : 10_MB;
	*buffer = (unsigned char *)malloc(allocSize);
	if (*buffer == nullptr)
	{
		WARN_LOG(NETWORK, "Memory alloc failed");
		*len = 0;
		return false;
	}
	Serializer ser(*buffer, allocSize, true);
	try {
		ser << frame;
		dc_serialize(ser);
		*len = ser.size();
	} catch (const Serializer::Exception& e) {
		WARN_LOG(NETWORK, "Save state failed: %s", e.what());
		*len = 0;
		return false;
	}
#ifdef SYNC_TEST
	*checksum = XXH3_64bits(*buffer, *len);
#endif
	memwatch::protect();
	if (frame > 0)
	{
#ifdef SYNC_TEST
		if (deltaStates.count(frame - 1) != 0)
		{
			MemPages memPages;
			memPages.load();
			const MemPages& savedPages = deltaStates[frame - 1];
			//verify(memPages.ram.size() == savedPages.ram.size());
			if (memPages.ram.size() != savedPages.ram.size())
			{
				ERROR_LOG(NETWORK, "old ram size %d new %d", (u32)savedPages.ram.size(), (u32)memPages.ram.size());
				if (memPages.ram.size() > savedPages.ram.size())
					for (const auto& pair : memPages.ram)
					{
						if (savedPages.ram.count(pair.first) == 0)
							ERROR_LOG(NETWORK, "new page @ %x", pair.first);
						else
							DEBUG_LOG(NETWORK, "page ok @ %x", pair.first);
					}
				die("fatal");
			}
			for (const auto& pair : memPages.ram)
			{
				verify(savedPages.ram.count(pair.first) == 1);
				verify(memcmp(&pair.second.data[0], &savedPages.ram.find(pair.first)->second.data[0], PAGE_SIZE) == 0);
			}
			verify(memPages.vram.size() == savedPages.vram.size());
			for (const auto& pair : memPages.vram)
			{
				verify(savedPages.vram.count(pair.first) == 1);
				verify(memcmp(&pair.second.data[0], &savedPages.vram.find(pair.first)->second.data[0], PAGE_SIZE) == 0);
			}
			//verify(memPages.aram.size() == savedPages.aram.size());
			if (memPages.aram.size() != savedPages.aram.size())
			{
				ERROR_LOG(NETWORK, "old aram size %d new %d", (u32)savedPages.aram.size(), (u32)memPages.aram.size());
				die("fatal");
			}
			for (const auto& pair : memPages.aram)
			{
				verify(savedPages.aram.count(pair.first) == 1);
				verify(memcmp(&pair.second.data[0], &savedPages.aram.find(pair.first)->second.data[0], PAGE_SIZE) == 0);
			}
		}
#endif
		// Save the delta to frame-1
		deltaStates[frame - 1].load();
		DEBUG_LOG(NETWORK, "Saved frame %d pages: %d ram, %d vram, %d eram, %d aica ram", frame - 1, (u32)deltaStates[frame - 1].ram.size(),
				(u32)deltaStates[frame - 1].vram.size(), (u32)deltaStates[frame - 1].elanram.size(), (u32)deltaStates[frame - 1].aram.size());
	}

	return true;
}

/*
 * log_game_state - Used in diagnostic testing.  The client should use
 * the ggpo_log function to write the contents of the specified save
 * state in a human readible form.
 */
static bool log_game_state(char *filename, unsigned char *buffer, int len)
{
#ifdef SYNC_TEST
	static int lastLoggedFrame = -1;
	static u8 *lastState;
	int frame = *(u32 *)buffer;
	DEBUG_LOG(NETWORK, "log_game_state frame %d len %d", frame, len);
	if (lastLoggedFrame == frame) {
		for (int i = 0; i < len; i++)
			if (buffer[i] != lastState[i])
			{
				WARN_LOG(NETWORK, "States for frame %d differ at offset %d: now %x prev %x", frame, i, *(u32 *)&buffer[i & ~3], *(u32 *)&lastState[i & ~3]);
				break;
			}
	}
	lastState = buffer;
	lastLoggedFrame = frame;
#endif

	return true;
}

/*
 * free_buffer - Frees a game state allocated in save_game_state.  You
 * should deallocate the memory contained in the buffer.
 */
static void free_buffer(void *buffer)
{
	if (buffer != nullptr)
	{
		Deserializer deser(buffer, 1_MB, true);
		int frame;
		deser >> frame;
		deltaStates.erase(frame);
		free(buffer);
	}
}

static void on_message(u8 *msg, int len)
{
	if (len == 0)
		return;
	GameEvent *event = (GameEvent *)msg;
	switch (event->type)
	{
	case GameEvent::Chat:
		if (chatCallback != nullptr && GameEvent::chatMessageLen(len) > 0)
			chatCallback(event->u.chat.playerNum, std::string(event->u.chat.message, GameEvent::chatMessageLen(len)));
		break;

	case GameEvent::VF4Card:
		setRfidCardData(event->u.card.playerNum, event->u.card.data);
		break;

	default:
		WARN_LOG(NETWORK, "Unknown app message type %d", event->type);
		break;
	}
}

void startSession(int localPort, int localPlayerNum)
{
	GGPOSessionCallbacks cb{};
	cb.begin_game      = begin_game;
	cb.advance_frame   = advance_frame;
	cb.load_game_state = load_game_state;
	cb.save_game_state = save_game_state;
	cb.free_buffer     = free_buffer;
	cb.on_event        = on_event;
	cb.log_game_state  = log_game_state;
	cb.on_message      = on_message;

#ifdef SYNC_TEST
	GGPOErrorCode result = ggpo_start_synctest(&ggpoSession, &cb, settings.content.gameId.c_str(), MAX_PLAYERS, sizeof(kcode[0]), 1);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start sync session failed: %d", result);
		ggpoSession = nullptr;
		throw FlycastException("GGPO start sync session failed");
	}
	ggpo_idle(ggpoSession, 0);
	ggpo::localPlayerNum = localPlayerNum;
	GGPOPlayer player{ sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1 };
	result = ggpo_add_player(ggpoSession, &player, &localPlayer);
	player.player_num = (1 - localPlayerNum) + 1;
	result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
	synchronized = true;
	analogAxes = 0;
	NOTICE_LOG(NETWORK, "GGPO synctest session started");
#else
	if (settings.platform.isConsole())
		analogAxes = config::GGPOAnalogAxes;
	else
	{
		analogAxes = 0;
		absPointerPos = false;
		keyboardGame = false;
		mouseGame = false;
		if (settings.input.lightgunGame)
			absPointerPos = true;
		else if (settings.input.keyboardGame)
			keyboardGame = true;
		else if (settings.input.mouseGame)
			mouseGame = true;
		else if (NaomiGameInputs != nullptr)
		{
			for (const auto& axis : NaomiGameInputs->axes)
			{
				if (axis.name == nullptr)
					break;
				if (axis.type == Full)
					analogAxes = std::max(analogAxes, (int)axis.axis + 1);
			}
		}
		NOTICE_LOG(NETWORK, "GGPO: Using %d full analog axes", analogAxes);
	}
	inputSize = sizeof(kcode[0]) + analogAxes + (int)absPointerPos * sizeof(Inputs::u.absPos)
		+ (int)keyboardGame * sizeof(Inputs::u.keys) + (int)mouseGame * sizeof(Inputs::u.relPos);

	VerificationData verif;
	MD5Sum().add(settings.network.md5.bios)
			.add(settings.network.md5.game)
			.getDigest(verif.gameMD5);
	auto& digest = settings.network.md5.savestate;
	if (std::find_if(std::begin(digest), std::end(digest), [](u8 b) { return b != 0; }) != std::end(digest))
		memcpy(verif.stateMD5, digest, sizeof(digest));
	else
	{
		MD5Sum().add(settings.network.md5.nvmem)
				.add(settings.network.md5.nvmem2)
				.add(settings.network.md5.eeprom)
				.add(settings.network.md5.vmu)
				.getDigest(verif.stateMD5);
	}

	GGPOErrorCode result = ggpo_start_session(&ggpoSession, &cb, settings.content.gameId.c_str(), MAX_PLAYERS, inputSize, localPort,
			&verif, sizeof(verif));
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start session failed: %d", result);
		ggpoSession = nullptr;
		throw FlycastException("GGPO network initialization failed");
	}

	// automatically disconnect clients after 3000 ms and start our count-down timer
	// for disconnects after 1000 ms.   To completely disable disconnects, simply use
	// a value of 0 for ggpo_set_disconnect_timeout.
	ggpo_set_disconnect_timeout(ggpoSession, 3000);
	ggpo_set_disconnect_notify_start(ggpoSession, 1000);

	ggpo::localPlayerNum = localPlayerNum;
	GGPOPlayer player{ sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1 };
	result = ggpo_add_player(ggpoSession, &player, &localPlayer);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO cannot add local player: %d", result);
		stopSession();
		throw FlycastException("GGPO cannot add local player");
	}
	ggpo_set_frame_delay(ggpoSession, localPlayer, config::GGPODelay.get());

	size_t colon = config::NetworkServer.get().find(':');
	std::string peerIp = config::NetworkServer.get().substr(0, colon);
	if (peerIp.empty())
		peerIp = "127.0.0.1";
	u32 peerPort;
	if (colon == std::string::npos)
	{
		if (peerIp == "127.0.0.1")
			peerPort = localPort ^ 1;
		else
			peerPort = SERVER_PORT;
	}
	else
	{
		peerPort = atoi(config::NetworkServer.get().substr(colon + 1).c_str());
	}
	player.type = GGPO_PLAYERTYPE_REMOTE;
	// GGPO only takes a dotted IPv4 address: resolve host names here
	in_addr peerAddr;
	if (inet_pton(AF_INET, peerIp.c_str(), &peerAddr) != 1)
	{
		addrinfo hints {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		addrinfo *result = nullptr;
		if (getaddrinfo(peerIp.c_str(), nullptr, &hints, &result) != 0 || result == nullptr)
		{
			stopSession();
			throw FlycastException(strprintf(i18n::T("GGPO: unknown peer address %s"), peerIp.c_str()));
		}
		char address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &((sockaddr_in *)result->ai_addr)->sin_addr, address, sizeof(address));
		freeaddrinfo(result);
		NOTICE_LOG(NETWORK, "GGPO: peer %s resolved to %s", peerIp.c_str(), address);
		peerIp = address;
	}
	strcpy(player.u.remote.ip_address, peerIp.c_str());
	player.u.remote.port = peerPort;
	player.player_num = (1 - localPlayerNum) + 1;
	result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO cannot add remote player: %d", result);
		stopSession();
		throw FlycastException("GGPO cannot add remote player");
	}
	DEBUG_LOG(NETWORK, "GGPO session started");
#endif
}

void stopSession()
{
	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
		return;
	ggpo_close_session(ggpoSession);
	ggpoSession = nullptr;
	miniupnp.Term();
	emu.setNetworkState(false);
	memwatch::unprotect();
	memwatch::reset();
}

void getInput(MapleInputState inputState[4])
{
	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
	{
		getLocalInput(inputState);
		return;
	}
	for (int player = 0; player < 4; player++)
		inputState[player] = {};

	std::vector<u8> inputData(inputSize * MAX_PLAYERS);
	// should not call any callback
	GGPOErrorCode error = ggpo_synchronize_input(ggpoSession, (void *)&inputData[0], inputData.size(), nullptr);
	if (error != GGPO_OK)
	{
		stopSession();
		throw FlycastException("GGPO error");
	}

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		MapleInputState& state = inputState[player];
		const Inputs *inputs = (Inputs *)&inputData[player * inputSize];
		state.kcode = ~inputs->kcode;
		if (analogAxes > 0)
		{
			state.fullAxes[PJAI_X1] = inputs->u.analog.x << 8;
			if (analogAxes >= 2)
				state.fullAxes[PJAI_Y1] = inputs->u.analog.y << 8;
		}
		else if (absPointerPos)
		{
			state.absPos.x = inputs->u.absPos.x;
			state.absPos.y = inputs->u.absPos.y;
		}
		else if (keyboardGame)
		{
			memcpy(state.keyboard.key, inputs->u.keys, sizeof(state.keyboard.key));
			state.keyboard.shift = inputs->kbModifiers;
		}
		else if (mouseGame)
		{
			state.relPos.x = inputs->u.relPos.x;
			state.relPos.y = inputs->u.relPos.y;
			state.relPos.wheel = inputs->u.relPos.wheel;
			state.mouseButtons = ~inputs->mouseButtons;
		}
		state.halfAxes[PJTI_R] = (state.kcode & BTN_TRIGGER_RIGHT) == 0 ? 0xffff : 0;
		state.halfAxes[PJTI_L] = (state.kcode & BTN_TRIGGER_LEFT) == 0 ? 0xffff : 0;
	}
}

// ---- KOFXI rollback bench (temporary): GGPO's save / load / resimulate cycle without a network ----
struct BenchSaved { unsigned char *buf = nullptr; int len = 0; int frame = 0; };
static std::deque<BenchSaved> benchRing;
static int benchFrameNo = 0, benchRollbacks = 0, benchMismatch = 0, benchRamMismatch = 0;
static double benchRollbackMs = 0, benchRollbackMaxMs = 0;

static u64 benchMemHash() {
	u64 h = XXH3_64bits(&mem_b[0], RAM_SIZE);
	h ^= XXH3_64bits(&vram[0], VRAM_SIZE) * 3;
	h ^= XXH3_64bits(&aica::aica_ram[0], ARAM_SIZE) * 5;
	return h;
}

static void benchAdvanceFrame() {
	settings.aica.muteAudio = true;
	rend_enable_renderer(false);
	inRollback = true;
	auto a0 = std::chrono::steady_clock::now();
	emu.run();
	auto a1 = std::chrono::steady_clock::now();
	benchAdvance.add((double)std::chrono::duration_cast<std::chrono::nanoseconds>(a1 - a0).count() / 1e6);
	settings.aica.muteAudio = false;
	rend_enable_renderer(true);
	inRollback = false;
	_endOfFrame = false;
}

static void bstep(const char* m, int f) {
	static FILE* q = std::fopen("bench_progress.txt", "w");
	if (q) { std::fprintf(q, "%d %s\n", f, m); std::fflush(q); }
}

static bool benchFrame() {
	static const int K = [] { int k = std::atoi(std::getenv("KOFXI_ROLLBACK_BENCH")); return k > 0 ? k : 8; }();
	int f = benchFrameNo++;
	bstep("begin", f);
	BenchSaved s; s.frame = f; int chk = 0;
	if (!save_game_state(&s.buf, &s.len, &chk, f))
		return false;
	bstep("saved", f);
	benchRing.push_back(s);
	while ((int)benchRing.size() > K + 1) { free_buffer(benchRing.front().buf); benchRing.pop_front(); }
	if (f >= K && f % 120 == 0 && (int)benchRing.size() == K + 1)
	{
		const std::vector<unsigned char> before(benchRing.back().buf, benchRing.back().buf + benchRing.back().len);
		const u64 memBefore = benchMemHash();
		bstep("before-load", f);
		double t0 = benchNow();
		load_game_state(benchRing.front().buf, benchRing.front().len);
		bstep("loaded", f);
		for (int i = 1; i <= K; i++)
		{
			bstep("advance", i);
			benchAdvanceFrame();
			bstep("advanced", i);
			BenchSaved n; n.frame = f - K + i; int c2 = 0;
			if (!save_game_state(&n.buf, &n.len, &c2, n.frame))
				return false;
			free_buffer(benchRing[i].buf);
			benchRing[i] = n;
		}
		double dt = benchNow() - t0;
		benchRollbackMs += dt; if (dt > benchRollbackMaxMs) benchRollbackMaxMs = dt;
		++benchRollbacks;
		const BenchSaved& after = benchRing.back();
		bool same = (int)before.size() == after.len && std::memcmp(before.data(), after.buf, after.len) == 0;
		bool ramSame = benchMemHash() == memBefore;
		if (!same) ++benchMismatch;
		if (!ramSame) ++benchRamMismatch;
		benchReport();
		static FILE* f2 = std::fopen((std::string(std::getenv("KOFXI_ROLLBACK_LOG") ? std::getenv("KOFXI_ROLLBACK_LOG") : "rollback_bench.txt") + ".rollbacks").c_str(), "w");
		if (f2) {
			std::fprintf(f2, "frame %d K=%d rollback+resim %.2f ms (total %d, avg %.2f, max %.2f) state_same=%d mem_same=%d mismatches state=%d mem=%d\n",
				f, K, dt, benchRollbacks, benchRollbackMs / benchRollbacks, benchRollbackMaxMs, same, ramSame, benchMismatch, benchRamMismatch);
			std::fflush(f2);
		}
	}
	return true;
}

// Bench B: no memwatch. Per-frame rollback-mode save cost, dirty pages per frame (shadow copy) and a
// full save / run K / restore / run K determinism check every 120 frames.
static u8 *shadowRam = nullptr, *shadowVram = nullptr, *shadowAram = nullptr;
static double bSaveMs = 0, bSaveMax = 0, bCopyMs = 0; static u64 bSaveN = 0, bBytes = 0, bPages[3] = {0, 0, 0};
static double bFullSave = 0, bFullLoad = 0, bFrameMs = 0; static u64 bFrames = 0;
static int bTests = 0, bMismatchState = 0, bMismatchMem = 0;
static size_t bFullSize = 0;

static u64 benchStateHash() {
	static std::vector<unsigned char> tmp(24 * 1024 * 1024);
	Serializer ser(tmp.data(), tmp.size(), true);
	dc_serialize(ser);
	return XXH3_64bits(tmp.data(), ser.size());
}
static u32 dirtyPages(const u8 *cur, u8 *shadow, u32 size) {
	u32 n = 0;
	for (u32 o = 0; o < size; o += PAGE_SIZE)
		if (std::memcmp(cur + o, shadow + o, PAGE_SIZE) != 0) { ++n; std::memcpy(shadow + o, cur + o, PAGE_SIZE); }
	return n;
}
static void benchLine(FILE* f, const char* label) { std::fprintf(f, "%s", label); std::fputc(10, f); std::fflush(f); }

static bool benchFrame2() {
	static const int K = [] { int k = std::atoi(std::getenv("KOFXI_ROLLBACK_BENCH")); return k > 0 ? k : 8; }();
	static FILE* out = std::fopen((std::string(std::getenv("KOFXI_ROLLBACK_LOG") ? std::getenv("KOFXI_ROLLBACK_LOG") : "rollback_bench.txt")).c_str(), "w");
	static std::vector<unsigned char> buf(24 * 1024 * 1024);
	static std::vector<unsigned char> full;
	if (!shadowRam) {
		shadowRam = (u8*)calloc(1, RAM_SIZE); shadowVram = (u8*)calloc(1, VRAM_SIZE); shadowAram = (u8*)calloc(1, ARAM_SIZE);
	}
	int f = benchFrameNo++;
	// 1. what GGPO saves every frame: non-memory state ...
	double t0 = benchNow();
	Serializer ser(buf.data(), buf.size(), true);
	dc_serialize(ser);
	double dt = benchNow() - t0;
	bSaveMs += dt; if (dt > bSaveMax) bSaveMax = dt; ++bSaveN; bBytes += ser.size();
	// 2. ... plus the pages that changed (memwatch would copy exactly these)
	u32 pr = dirtyPages(&mem_b[0], shadowRam, RAM_SIZE), pv = dirtyPages(&vram[0], shadowVram, VRAM_SIZE),
		pa = dirtyPages(&aica::aica_ram[0], shadowAram, ARAM_SIZE);
	bPages[0] += pr; bPages[1] += pv; bPages[2] += pa;
	{
		static std::vector<u8> sink(4096 * 4096);
		double c0 = benchNow();
		u32 total = std::min<u32>(pr + pv + pa, 4096);
		for (u32 i = 0; i < total; i++) std::memcpy(&sink[i * 4096], &mem_b[(i * 4096) % RAM_SIZE], 4096);
		bCopyMs += benchNow() - c0;
	}
	if (f % 10 == 0 && out) { char l2[200]; std::snprintf(l2, sizeof l2, "progress f=%d save %.2f ms bytes %zu dirty ram %u vram %u aram %u", f, dt, (size_t)ser.size(), pr, pv, pa); benchLine(out, l2); }
	if (f > 0 && f % 120 == 0 && f > K)
	{
		// 3. determinism: full state, run K, hash, restore, run K, hash
		Serializer sz;
		dc_serialize(sz);
		full.resize(sz.size());
		double s0 = benchNow();
		Serializer sf(full.data(), full.size());
		dc_serialize(sf);
		bFullSave += benchNow() - s0; bFullSize = sz.size();
		for (int i = 0; i < K; i++) { double a = benchNow(); benchAdvanceFrame(); bFrameMs += benchNow() - a; ++bFrames; }
		const u64 h1 = benchStateHash(), m1 = benchMemHash();
		double l0 = benchNow();
		{
			Deserializer dz(full.data(), full.size());
			emu.loadstate(dz);
		}
		bFullLoad += benchNow() - l0;
		for (int i = 0; i < K; i++) { double a = benchNow(); benchAdvanceFrame(); bFrameMs += benchNow() - a; ++bFrames; }
		const u64 h2 = benchStateHash(), m2 = benchMemHash();
		++bTests; if (h1 != h2) ++bMismatchState; if (m1 != m2) ++bMismatchMem;
		memset(shadowRam, 0, RAM_SIZE); memset(shadowVram, 0, VRAM_SIZE); memset(shadowAram, 0, ARAM_SIZE);
		char line[640];
		std::snprintf(line, sizeof line,
			"frame %d | per-frame (rollback-mode save): %.3f ms avg %.3f max %.3f, %.0f B/frame | dirty pages/frame ram %.1f vram %.1f aram %.1f, copy of those %.3f ms/frame | "
			"full save %.2f ms (%zu B) full load %.2f ms | resim frame (no render, no audio) %.2f ms avg (%llu frames) max %.2f | K=%d tests %d state_mismatch %d mem_mismatch %d",
			f, dt, bSaveMs / bSaveN, bSaveMax, (double)bBytes / bSaveN, (double)bPages[0] / bSaveN, (double)bPages[1] / bSaveN,
			(double)bPages[2] / bSaveN, bCopyMs / bSaveN, bFullSave / bTests, bFullSize, bFullLoad / bTests,
			benchAdvance.n ? benchAdvance.ms / benchAdvance.n : 0.0, (unsigned long long)benchAdvance.n, benchAdvance.maxMs, K, bTests, bMismatchState, bMismatchMem);
		if (out) benchLine(out, line);
	}
	return true;
}

// ---- Bench C: two-sided inputs (Fightcade replay), GGPO-style prediction and rollback ----
// Pass A: straight run with the true inputs (reference hashes). Pass B(L): the same run as seen by P1 when
// P2's input arrives L frames late and is predicted as "same as the last confirmed one". On a wrong
// prediction: restore the state at the start of the frame, re-run to the present with corrected inputs.
// Memory is restored page by page from per-frame deltas (what memwatch would give), the rest through
// dc_deserialize in rollback mode. Renderer and audio are off for every frame: this is emulation-thread cost.
struct BenchInput { u32 p1 = 0, p2 = 0; };
struct BenchDelta { struct Page { u8 region; u32 index; std::array<u8, 4096> old; }; std::vector<Page> pages; };

static u8* benchRegion(int r) { return r == 0 ? &mem_b[0] : r == 1 ? &vram[0] : &aica::aica_ram[0]; }
static u32 benchRegionSize(int r) { return r == 0 ? (u32)RAM_SIZE : r == 1 ? (u32)VRAM_SIZE : (u32)ARAM_SIZE; }

struct BenchCtx
{
	std::vector<BenchInput> in;
	u8* shadow[3] = {};
	static constexpr int RING = 64;
	std::vector<std::vector<unsigned char>> state{RING};
	std::vector<BenchDelta> delta{RING};
	std::vector<u32> used2;
	std::vector<u64> hashA;
	void resetShadow() {
		for (int r = 0; r < 3; r++) {
			if (!shadow[r]) shadow[r] = (u8*)malloc(benchRegionSize(r));
			std::memcpy(shadow[r], benchRegion(r), benchRegionSize(r));
		}
	}
};

static long long benchNs() {
	static const auto base = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - base).count();
}

// Executes frame f with the given inputs, saving its start state and recording the memory deltas.
static long long benchExec(BenchCtx& c, int f, u32 p1, u32 p2)
{
	long long t0 = benchNs();
	auto& st = c.state[f % BenchCtx::RING];
	st.resize(2 * 1024 * 1024);
	Serializer ser(st.data(), st.size(), true);
	dc_serialize(ser);
	st.resize(ser.size());
	long long t1 = benchNs();
	kcode[0] = ~p1; kcode[1] = ~p2;
	benchAdvanceFrame();
	long long t2 = benchNs();
	// dirty pages: the scan is not timed (memwatch gets them from page faults); the copy is
	BenchDelta& d = c.delta[f % BenchCtx::RING];
	d.pages.clear();
	std::vector<std::pair<u8, u32>> dirty;
	for (int r = 0; r < 3; r++) {
		const u8* cur = benchRegion(r); const u32 size = benchRegionSize(r);
		for (u32 o = 0; o < size; o += 4096)
			if (std::memcmp(cur + o, c.shadow[r] + o, 4096) != 0) dirty.push_back({(u8)r, o / 4096});
	}
	long long t3 = benchNs();
	d.pages.resize(dirty.size());
	for (size_t i = 0; i < dirty.size(); i++) {
		auto& pg = d.pages[i]; pg.region = dirty[i].first; pg.index = dirty[i].second;
		std::memcpy(pg.old.data(), c.shadow[pg.region] + pg.index * 4096, 4096);
		std::memcpy(c.shadow[pg.region] + pg.index * 4096, benchRegion(pg.region) + pg.index * 4096, 4096);
	}
	long long t4 = benchNs();
	return (t1 - t0) + (t2 - t1) + (t4 - t3);
}

// Restores the state at the start of frame `to`, being at the start of frame `from` (> to).
static long long benchRestore(BenchCtx& c, int from, int to)
{
	long long t0 = benchNs();
	for (int g = from - 1; g >= to; g--)
		for (auto& pg : c.delta[g % BenchCtx::RING].pages) {
			std::memcpy(benchRegion(pg.region) + pg.index * 4096, pg.old.data(), 4096);
			std::memcpy(c.shadow[pg.region] + pg.index * 4096, pg.old.data(), 4096);
		}
	auto& st = c.state[to % BenchCtx::RING];
	Deserializer deser(st.data(), st.size(), true);
	dc_deserialize(deser);
	return benchNs() - t0;
}

__declspec(noinline) static void benchPercentiles(std::vector<long long> v, FILE* out, const char* name)
{
	if (v.empty()) return;
	std::sort(v.begin(), v.end());
	auto q = [&](double p) { return (double)v[std::min(v.size() - 1, (size_t)(p * v.size()))] / 1e6; };
	u64 over = 0, over2 = 0; double sum = 0;
	for (long long n : v) { double x = (double)n / 1e6; sum += x; if (x > 16.67) ++over; if (x > 33.3) ++over2; }
	std::fprintf(out, "  %s: mean %.2f p50 %.2f p90 %.2f p95 %.2f p99 %.2f max %.2f ms | >16.7 ms: %.1f%% >33 ms: %.1f%% (n=%zu)", name, sum / v.size(), q(.5), q(.9), q(.95), q(.99), (double)v.back() / 1e6,
			100.0 * over / v.size(), 100.0 * over2 / v.size(), v.size());
	std::fputc(10, out);
}

// ---- Diagnosis: why is the restore not exact? Ladder T0..T3, per-page hashes of RAM / VRAM / ARAM ----
static std::vector<u64> benchPageHashes() {
	std::vector<u64> v;
	for (int r = 0; r < 3; r++) {
		const u8* q = benchRegion(r); const u32 n = benchRegionSize(r) / 4096;
		for (u32 i = 0; i < n; i++) v.push_back(XXH3_64bits(q + i * 4096, 4096));
	}
	return v;
}
__declspec(noinline) static void benchDiffReport(FILE* out, const char* tag, int f, const std::vector<u64>& a, const std::vector<u64>& b) {
	if (a == b) return;
	u32 cnt[3] = {0, 0, 0}; std::string first;
	size_t k = 0;
	for (int r = 0; r < 3; r++) {
		const u32 n = benchRegionSize(r) / 4096;
		for (u32 i = 0; i < n; i++, k++)
			if (a[k] != b[k]) { cnt[r]++; if (first.size() < 200) { char t[48]; std::snprintf(t, sizeof t, " %s+0x%x", r == 0 ? "RAM" : r == 1 ? "VRAM" : "ARAM", i * 4096); first += t; } }
	}
	std::fprintf(out, "  [%s] frame %d differs: pages ram %u vram %u aram %u; first:%s", tag, f, cnt[0], cnt[1], cnt[2], first.c_str());
	std::fputc(10, out);
}
// Runs frames [from, to) with the replay inputs; returns per-frame page hashes.
static void benchRunFrames(BenchCtx& c, int from, int to, std::vector<std::vector<u64>>& hashes) {
	for (int f = from; f < to; f++) {
		kcode[0] = ~c.in[f].p1; kcode[1] = ~c.in[f].p2;
		benchAdvanceFrame();
		hashes[f] = benchPageHashes();
	}
}
// Compares a run against the reference; returns first differing frame or -1
static int benchCompare(FILE* out, const char* tag, int from, int to, const std::vector<std::vector<u64>>& ref, const std::vector<std::vector<u64>>& run) {
	int firstBad = -1, bad = 0;
	for (int f = from; f < to; f++)
		if (ref[f] != run[f]) { if (firstBad < 0) { firstBad = f; benchDiffReport(out, tag, f, ref[f], run[f]); } ++bad; }
	std::fprintf(out, "%s: %d of %d frames differ%s", tag, bad, to - from, firstBad >= 0 ? "" : " (all identical)");
	std::fputc(10, out); std::fflush(out);
	return firstBad;
}
__declspec(noinline) static void benchTimes(FILE* out, const char* tag, long long restoreNs, long long runNs, int frames) {
	std::fprintf(out, "  [%s] restore %.2f ms, then %d frames in %.1f ms (%.2f ms/frame, first included)", tag, (double)restoreNs / 1e6, frames, (double)runNs / 1e6, (double)runNs / 1e6 / frames);
	std::fputc(10, out);
}
static void benchDiag(BenchCtx& c, FILE* out)
{
	const int K = 60, F = 100;
	std::vector<std::vector<u64>> ref(F), r2(F), run(F);
	std::vector<unsigned char> fullState, rbState;
	u8* memCopy[3];
	for (int r = 0; r < 3; r++) memCopy[r] = (u8*)malloc(benchRegionSize(r));
	// R1: reference, saving states and memory at the start of frame K
	dc_loadstate(0);
	benchRunFrames(c, 0, K, ref);
	{
		Serializer sz; dc_serialize(sz); fullState.resize(sz.size());
		Serializer sf(fullState.data(), fullState.size()); dc_serialize(sf);
		rbState.resize(4 * 1024 * 1024);
		Serializer sr(rbState.data(), rbState.size(), true); dc_serialize(sr); rbState.resize(sr.size());
		for (int r = 0; r < 3; r++) std::memcpy(memCopy[r], benchRegion(r), benchRegionSize(r));
	}
	benchRunFrames(c, K, F, ref);
	std::fprintf(out, "DIAG state sizes: full %zu, rollback-mode %zu bytes", fullState.size(), rbState.size());
	std::fputc(10, out);
	// T0: straight run again from the same savestate
	dc_loadstate(0);
	benchRunFrames(c, 0, F, r2);
	benchCompare(out, "T0 straight run twice (determinism of the emulation itself)", 0, F, ref, r2);
	// T1: full-state restore (emu.loadstate: flushes dynarec, mmu)
	{
		Deserializer dz(fullState.data(), fullState.size());
		emu.loadstate(dz);
	}
	benchRunFrames(c, K, F, run);
	benchCompare(out, "T1 full state via emu.loadstate", K, F, ref, run);
	// T2: rollback-mode deserialize + exact memory copy (no dynarec flush)
	for (int r = 0; r < 3; r++) std::memcpy(benchRegion(r), memCopy[r], benchRegionSize(r));
	{
		Deserializer dr(rbState.data(), rbState.size(), true);
		dc_deserialize(dr);
	}
	for (auto& v : run) v.clear();
	benchRunFrames(c, K, F, run);
	benchCompare(out, "T2 rollback-mode dc_deserialize + exact memory (what GGPO does, no dynarec reset)", K, F, ref, run);
	// T3a..d: rollback-mode deserialize + exact memory + each piece of the reset on its own
	static const char* names[4] = {"T3a bm_Reset only", "T3b ResetCache only", "T3c mmu_flush_table+mmu_set_state only", "T3d bm_Reset + ResetCache (no mmu)"};
	for (int v = 0; v < 4; v++) {
		for (int r = 0; r < 3; r++) std::memcpy(benchRegion(r), memCopy[r], benchRegionSize(r));
		long long t0 = benchNs();
		if (v == 0 || v == 3) bm_Reset();
		if (v == 2) mmu_flush_table();
		{
			Deserializer dr(rbState.data(), rbState.size(), true);
			dc_deserialize(dr);
		}
		if (v == 2) mmu_set_state();
		if (v == 1 || v == 3) emu.getSh4Executor()->ResetCache();
		long long t1 = benchNs();
		for (auto& vv : run) vv.clear();
		benchRunFrames(c, K, F, run);
		long long t2 = benchNs();
		benchCompare(out, names[v], K, F, ref, run);
		benchTimes(out, names[v], t1 - t0, t2 - t1, F - K);
	}
	// T4: exact memory + rollback-mode deserialize, and only the blocks of RAM pages that changed are invalidated
	for (int variant = 0; variant < 4; variant++) {
		// make the current state differ: the run above left RAM at frame F, memCopy is frame K
		std::vector<u32> changed;
		for (u32 o = 0; o < (u32)RAM_SIZE; o += 4096)
			if (std::memcmp(&mem_b[o], memCopy[0] + o, 4096) != 0) changed.push_back(o);
		for (int r = 0; r < 3; r++) std::memcpy(benchRegion(r), memCopy[r], benchRegionSize(r));
		long long t0 = benchNs();
		{
			Deserializer dr(rbState.data(), rbState.size(), true);
			dc_deserialize(dr);
		}
		// variant 0: invalidate the restored pages; variant 1: nothing (control, same pages restored again)
		if (variant == 0)
			for (u32 o : changed) bm_RamWriteAccess(o);
		if (variant >= 2)
			for (u32 o = 0; o < (u32)RAM_SIZE; o += 4096) bm_RamWriteAccess(o);
		if (variant == 3)
			bm_ResetTempCache(true);
		long long t1 = benchNs();
		for (auto& vv : run) vv.clear();
		benchRunFrames(c, K, F, run);
		long long t2 = benchNs();
		char tag[96];
		static const char* fm[4] = {"T4a invalidate %zu restored RAM pages only", "T4b control: no invalidation (%zu pages)", "T4c invalidate ALL RAM pages (%zu changed)", "T4d invalidate all RAM pages + bm_ResetTempCache (%zu changed)"};
		std::snprintf(tag, sizeof tag, fm[variant], changed.size());
		benchCompare(out, tag, K, F, ref, run);
		benchTimes(out, tag, t1 - t0, t2 - t1, F - K);
	}
	// T5: exact memory + rollback-mode deserialize, then pieces of ResetCache() by mask:
	// 1 codeBuffer.reset  2 bm_ResetCache (backend reset, fpcb reset, discard blocks)  4 smc_hotspots  8 temp cache  16 backend reset only  32 fpcb reset only
	static const unsigned masks[] = {2};
	for (unsigned mask : masks) {
		for (int r = 0; r < 3; r++) std::memcpy(benchRegion(r), memCopy[r], benchRegionSize(r));
		{
			Deserializer dr(rbState.data(), rbState.size(), true);
			dc_deserialize(dr);
		}
		kofxiBenchReset(mask);
		for (auto& vv : run) vv.clear();
		benchRunFrames(c, K, F, run);
		char tag[64];
		std::snprintf(tag, sizeof tag, "T5 mask %u", mask);
		benchCompare(out, tag, K, F, ref, run);
	}
	// T6: invalidate with the unprotected flag forced off first, so bm_RamWriteAccess cannot return early
	for (int variant = 0; variant < 2; variant++) {
		std::vector<u32> changed;
		for (u32 o = 0; o < (u32)RAM_SIZE; o += 4096)
			if (variant == 1 || std::memcmp(&mem_b[o], memCopy[0] + o, 4096) != 0) changed.push_back(o);
		for (int r = 0; r < 3; r++) std::memcpy(benchRegion(r), memCopy[r], benchRegionSize(r));
		{
			Deserializer dr(rbState.data(), rbState.size(), true);
			dc_deserialize(dr);
		}
		long long t0 = benchNs();
		u32 wasUnprot = 0;
		for (u32 o : changed) { if (unprotected_pages[o / 4096]) ++wasUnprot; unprotected_pages[o / 4096] = false; bm_RamWriteAccess(o); }
		long long t1 = benchNs();
		for (auto& vv : run) vv.clear();
		benchRunFrames(c, K, F, run);
		long long t2 = benchNs();
		char tag[128];
		std::snprintf(tag, sizeof tag, variant == 0 ? "T6a force-invalidate %zu restored RAM pages (%u were flagged unprotected)" : "T6b force-invalidate ALL %zu RAM pages (%u were flagged unprotected)", changed.size(), wasUnprot);
		benchCompare(out, tag, K, F, ref, run);
		benchTimes(out, tag, t1 - t0, t2 - t1, F - K);
	}
	std::fprintf(out, "DIAG done"); std::fputc(10, out); std::fflush(out);
}

__declspec(noinline) static void benchHeader(FILE* out, int pass, int L, int N, long long wallNs, long long rollbacks, long long resim, long long ok, long long bad)
{
	std::fprintf(out, "wall for pass: %.1f ms (%.3f ms per frame, includes hashing)", (double)wallNs / 1e6, (double)wallNs / 1e6 / N);
	std::fputc(10, out);
	if (pass < 0) std::fprintf(out, "pass A (reference, no rollback):");
	else std::fprintf(out, "pass B L=%d frames (%.0f ms): rollbacks %lld (%.1f%% of frames), resimulated frames %lld (%.2f per frame), determinism checks ok %lld bad %lld", L, L * 16.67,
			rollbacks, 100.0 * rollbacks / N, resim, (double)resim / N, ok, bad);
	std::fputc(10, out);
}

static void benchRunInputs()
{
	const char* csv = std::getenv("KOFXI_ROLLBACK_INPUTS");
	const char* logp = std::getenv("KOFXI_ROLLBACK_LOG");
	FILE* out = std::fopen(logp ? logp : "rollback_inputs.txt", "w");
	if (!out) return;
	BenchCtx c;
	{
		std::ifstream f(csv); std::string line; std::getline(f, line);
		while (std::getline(f, line)) {
			unsigned a = 0, b = 0, fr = 0;
			if (std::sscanf(line.c_str(), "%u,%u,%u", &fr, &a, &b) == 3) { BenchInput i; i.p1 = a; i.p2 = b; c.in.push_back(i); }
		}
	}
	const int N = std::min<int>((int)c.in.size(), std::getenv("KOFXI_ROLLBACK_FRAMES") ? std::atoi(std::getenv("KOFXI_ROLLBACK_FRAMES")) : 3000);
	const int WARM = 240;
	std::vector<int> lats;
	{ std::string s2 = std::getenv("KOFXI_ROLLBACK_LAT") ? std::getenv("KOFXI_ROLLBACK_LAT") : "2,4,8,15,30"; size_t p = 0;
	  while (p < s2.size()) { size_t e = s2.find(',', p); if (e == std::string::npos) e = s2.size(); lats.push_back(std::atoi(s2.substr(p, e - p).c_str())); p = e + 1; } }
	std::fprintf(out, "inputs %zu frames, running %d (warm-up %d excluded); passes: A (no rollback) then B(L) for L in the list", c.in.size(), N, WARM);
	std::fputc(10, out); std::fflush(out);
	c.hashA.assign(N, 0);
	if (std::getenv("KOFXI_ROLLBACK_DIAG")) { benchDiag(c, out); std::fclose(out); std::_Exit(0); }
	for (int pass = -1; pass < (int)lats.size(); pass++)
	{
		const int L = pass < 0 ? 0 : lats[pass];
		dc_loadstate(0);
		c.resetShadow();
		const long long passStart = benchNs();
		c.used2.assign(N + 1, 0);
		std::vector<long long> perFrame, execOnly;
		u64 rollbacks = 0, resim = 0, checksOk = 0, checksBad = 0;
		for (int f = 0; f < N; f++)
		{
			long long T = 0;
			const int cidx = f - L;
			if (pass >= 0 && L > 0 && cidx >= 0 && c.used2[cidx] != c.in[cidx].p2)
			{
				++rollbacks; resim += L;
				T += benchRestore(c, f, cidx);
				c.used2[cidx] = c.in[cidx].p2;
				for (int g = cidx; g < f; g++)
				{
					if (g > cidx) c.used2[g] = c.in[cidx].p2;	// prediction: the last confirmed input
					T += benchExec(c, g, c.in[g].p1, c.used2[g]);
					if (g == cidx) {
						if (benchMemHash() == c.hashA[g]) ++checksOk; else ++checksBad;
					}
				}
			}
			if (pass < 0 || L == 0) c.used2[f] = c.in[f].p2;
			else c.used2[f] = cidx >= 0 ? c.in[cidx].p2 : 0;
			long long e = benchExec(c, f, c.in[f].p1, c.used2[f]);
			T += e;
			if (pass < 0) c.hashA[f] = benchMemHash();
			if (f >= WARM) { perFrame.push_back(T); execOnly.push_back(e); }
		}
		benchHeader(out, pass, L, N, benchNs() - passStart, (long long)rollbacks, (long long)resim, (long long)checksOk, (long long)checksBad);
		benchPercentiles(perFrame, out, "emulation thread per frame (incl. rollback)");
		benchPercentiles(execOnly, out, "frame alone (save + execute + delta copy)");
		std::fflush(out);
	}
	std::fprintf(out, "done"); std::fputc(10, out); std::fclose(out);
	std::_Exit(0);
}

bool nextFrame()
{
	if (benchOn() && ggpoSession == nullptr)
	{
		if (!_endOfFrame)
			return false;
		_endOfFrame = false;
		if (inRollback)
			return true;
		if (std::getenv("KOFXI_ROLLBACK_INPUTS"))
			benchRunInputs();
		bool ok = std::getenv("KOFXI_ROLLBACK_MEMWATCH") ? benchFrame() : benchFrame2();
		if (ok)
			emu.getSh4Executor()->Start();
		return ok;
	}
	if (!_endOfFrame)
		return false;
	_endOfFrame = false;
	if (inRollback)
		return true;
	auto now = std::chrono::steady_clock::now();
	if (lastFrameTime != time_point<steady_clock>())
	{
		msPerFrame[msPerFrameIndex++] = duration_cast<milliseconds>(now - lastFrameTime).count();
		if (msPerFrameIndex >= (int)msPerFrame.size())
			msPerFrameIndex = 0;
		msPerFrameAvg = std::accumulate(msPerFrame.begin(), msPerFrame.end(), 0) / msPerFrame.size();
	}
	lastFrameTime = now;

	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
		return false;
	// will call save_game_state
	GGPOErrorCode error = ggpo_advance_frame(ggpoSession);

	// may rollback
	if (error == GGPO_OK)
		error = ggpo_idle(ggpoSession, 0);
	if (error != GGPO_OK)
	{
		stopSession();
		if (error == GGPO_ERRORCODE_INPUT_SIZE_DIFF)
			throw FlycastException("GGPO analog settings are different from peer");
		else
			throw FlycastException("GGPO error");
	}

	// may call save_game_state
	do {
		if (!config::ThreadedRendering)
			os_UpdateInputState();
		Inputs inputs;
		inputs.kcode = ~kcode[0];
		if (rt[0] >= 0x4000)
			inputs.kcode |= BTN_TRIGGER_RIGHT;
		else
			inputs.kcode &= ~BTN_TRIGGER_RIGHT;
		if (lt[0] >= 0x4000)
			inputs.kcode |= BTN_TRIGGER_LEFT;
		else
			inputs.kcode &= ~BTN_TRIGGER_LEFT;
		inputs.mouseButtons = 0;
		inputs.kbModifiers = 0;
		if (analogAxes > 0)
		{
			inputs.u.analog.x = joyx[0] >> 8;
			if (analogAxes >= 2)
				inputs.u.analog.y = joyy[0] >> 8;
		}
		else if (absPointerPos)
		{
			inputs.u.absPos.x = mo_x_abs[0];
			inputs.u.absPos.y = mo_y_abs[0];
		}
		else if (keyboardGame)
		{
			inputs.kbModifiers = kb_shift[0];
			memcpy(inputs.u.keys, kb_key[0], sizeof(kb_key[0]));
		}
		else if (mouseGame)
		{
			std::lock_guard<std::mutex> lock(relPosMutex);
			inputs.mouseButtons = ~mo_buttons[0];
			inputs.u.relPos.x = std::round(mo_x_delta[0]);
			inputs.u.relPos.y = std::round(mo_y_delta[0]);
			inputs.u.relPos.wheel = std::round(mo_wheel_delta[0]);
			mo_x_delta[0] -= inputs.u.relPos.x;
			mo_y_delta[0] -= inputs.u.relPos.y;
			mo_wheel_delta[0] -= inputs.u.relPos.wheel;
		}
		error = ggpo_add_local_input(ggpoSession, localPlayer, &inputs, inputSize);
		if (error == GGPO_OK)
			break;
		if (error != GGPO_ERRORCODE_PREDICTION_THRESHOLD)
		{
			WARN_LOG(NETWORK, "ggpo_add_local_input failed %d", error);
			stopSession();
			throw FlycastException("GGPO error");
		}
		DEBUG_LOG(NETWORK, "ggpo_add_local_input prediction barrier reached");
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		error = ggpo_idle(ggpoSession, 0);
		if (error != GGPO_OK)
		{
			stopSession();
			throw FlycastException("GGPO error");
		}
	} while (active());
#ifdef SYNC_TEST
	u32 input = ~kcode[1 - localPlayerNum];
	GGPOErrorCode result = ggpo_add_local_input(ggpoSession, remotePlayer, &input, sizeof(input));
	if (result != GGPO_OK)
		WARN_LOG(NETWORK, "ggpo_add_local_input(2) failed %d", result);
#endif
	if (active()) {
		emu.getSh4Executor()->Start();
		return true;
	}
	return false;
}

bool active()
{
	return ggpoSession != nullptr || benchOn();
}

std::future<bool> startNetwork()
{
	synchronized = false;
	return std::async(std::launch::async, []{
		{
			ThreadName _("GGPO-start");
			std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
#ifdef SYNC_TEST
			startSession(0, 0);
#else
			if (config::EnableUPnP)
			{
				miniupnp.Init();
				miniupnp.AddPortMapping(SERVER_PORT, false);
			}

			try {
				if (config::ActAsServer)
					startSession(SERVER_PORT, 0);
				else
					// Use SERVER_PORT-1 as local port if connecting to ourselves
					startSession(config::NetworkServer.get().empty() || config::NetworkServer.get() == "127.0.0.1" ? SERVER_PORT - 1 : SERVER_PORT, 1);
			} catch (...) {
				miniupnp.Term();
				throw;
			}
#endif
		}
		while (!synchronized && active())
		{
			{
				std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
				if (ggpoSession == nullptr)
					break;
				GGPOErrorCode result = ggpo_idle(ggpoSession, 0);
				if (result == GGPO_ERRORCODE_VERIFICATION_ERROR)
					throw FlycastException("Peer verification failed");
				else if (result != GGPO_OK)
				{
					WARN_LOG(NETWORK, "ggpo_idle failed %d", result);
					throw FlycastException("GGPO error");
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
#ifdef SYNC_TEST
		// save initial state (frame 0)
		if (active())
		{
			MapleInputState state[4];
			getInput(state);
		}
#endif
		if (active() && (settings.content.gameId == "VIRTUA FIGHTER 4 JAPAN"
				|| settings.content.gameId == "VF4 EVOLUTION JAPAN"
				|| settings.content.gameId == "VF4 FINAL TUNED JAPAN"))
		{
			// Send the local P1 card
			const u8 *cardData = getRfidCardData(0);
			if (cardData != nullptr)
			{
				GameEvent event;
				event.type = GameEvent::VF4Card;
				event.u.card.playerNum = config::ActAsServer ? 0 : 1;
				memcpy(event.u.card.data, cardData, sizeof(event.u.card.data));
				ggpo_send_message(ggpoSession, &event, sizeof(event.type) + sizeof(event.u.card), true);
			}
		}
		emu.setNetworkState(active());
		return active();
	});
}

void displayStats()
{
	if (!active())
		return;
	GGPONetworkStats stats;
	ggpo_get_network_stats(ggpoSession, remotePlayer, &stats);

	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ScaledVec2(95, 0));
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("##ggpostats", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
	ImguiStyleColor _2(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	// Send Queue
	ImGui::Text("Send Q");
	ImGui::ProgressBar(stats.network.send_queue_len / 10.f, ImVec2(-1, uiScaled(10.f)), "");

	// Frame Delay
	ImGui::Text("Delay");
	std::string delay = std::to_string(config::GGPODelay.get());
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(delay.c_str()).x);
	ImGui::Text("%s", delay.c_str());

	// Ping
	ImGui::Text("Ping");
	std::string ping = std::to_string(stats.network.ping);
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ping.c_str()).x);
	ImGui::Text("%s", ping.c_str());

	// Predicted Frames
	if (stats.sync.predicted_frames >= 7)
		// red
	    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1, 0, 0, 1));
	else if (stats.sync.predicted_frames >= 5)
		// yellow
	    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(.9f, .9f, .1f, 1));
	ImGui::Text("Predicted");
	ImGui::ProgressBar(stats.sync.predicted_frames / 7.f, ImVec2(-1, uiScaled(10.f)), "");
	if (stats.sync.predicted_frames >= 5)
		ImGui::PopStyleColor();

	// Frames behind
	int timesync = timesyncOccurred;
	if (timesync > 0)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
	ImGui::Text("Behind");
	ImGui::ProgressBar(0.5f + stats.timesync.local_frames_behind / 16.f, ImVec2(-1, uiScaled(10.f)), "");
	if (timesync > 0)
	{
		ImGui::PopStyleColor();
		timesyncOccurred--;
	}

	ImGui::End();
}

void endOfFrame()
{
	if (active())
	{
		_endOfFrame = true;
		emu.getSh4Executor()->Stop();
	}
}

void sendChatMessage(int playerNum, const std::string& msg) {
	if (!active())
		return;
	GameEvent event;
	event.type = GameEvent::Chat;
	event.u.chat.playerNum = playerNum;
	size_t msgLen = std::min(msg.length(), sizeof(event.u.chat.message));
	memcpy(event.u.chat.message, msg.c_str(), msgLen);
	ggpo_send_message(ggpoSession, &event, sizeof(GameEvent) - sizeof(event.u.chat.message) + msgLen, true);
}

void receiveChatMessages(void (*callback)(int playerNum, const std::string& msg))
{
	chatCallback = callback;
}

}

#else // LIBRETRO

namespace ggpo
{

void stopSession() {
}

void getInput(MapleInputState inputState[4])
{
	getLocalInput(inputState);
}

bool nextFrame() {
	return true;
}

bool active() {
	return false;
}

std::future<bool> startNetwork() {
	return std::async(std::launch::deferred, []{ return false; });
}

void displayStats() {
}

void endOfFrame() {
}

void sendChatMessage(int playerNum, const std::string& msg) {
}

void receiveChatMessages(void (*callback)(int playerNum, const std::string& msg)) {
}

}
#endif
