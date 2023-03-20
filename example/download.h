#pragma once

#include "pnrpc/net_server.h"
#include <string>

RPC_DECLARE(Download, std::string, std::string, 0x05, pnrpc::RpcType::ServerSideStream, OVERRIDE_PROCESS)