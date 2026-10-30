#pragma once
#include "maple_devs.h"
#include <memory>

extern std::shared_ptr<maple_device> MapleDevices[MAPLE_PORTS][6];

void maple_Init();
void maple_Reset(bool Manual);
void maple_Term();
void maple_ReconnectDevices();
void maple_ReconnectDevice(int bus, int port);

#if defined(_DEBUG) && defined(_WIN32)
struct MapleDiagState
{
	bool dmaStarted;
	u32 pendingResponses;
	bool callbackScheduled;
	bool drainCandidate() const { return !dmaStarted && pendingResponses == 0 && !callbackScheduled; }
};

MapleDiagState maple_get_diag_state();
#endif

void maple_vblank();
