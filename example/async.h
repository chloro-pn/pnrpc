#pragma once

#include "pnrpc/async_task.h"
#include "pnrpc/net_server.h"

RPC_DECLARE(Async, std::string, std::string, 0x03, pnrpc::RpcType::Simple, OVERRIDE_PROCESS)