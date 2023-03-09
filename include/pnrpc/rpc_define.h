#pragma once

#include "pnrpc/rpc_server.h"
#include "bridge/object.h"

#include <string>
#include <memory>

RPC_DECLARE(async_task, std::string, std::string, 0x03, OVERRIDE_PROCESS)