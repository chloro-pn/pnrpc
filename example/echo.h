#pragma once

#include "pnrpc/net_server.h"

RPC_DECLARE(Echo, std::string, std::string, 0x01, OVERRIDE_PROCESS)