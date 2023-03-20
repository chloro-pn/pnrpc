#pragma once

#include "pnrpc/net_server.h"

RPC_DECLARE(Echo, std::string, std::string, 0x01, pnrpc::RpcType::Simple, OVERRIDE_PROCESS)