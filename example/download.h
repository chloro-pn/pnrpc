#pragma once

#include "pnrpc/rpc_declare.h"
#include <string>

RPC_DECLARE(Download, std::string, std::string, 0x05, pnrpc::RpcType::ServerSideStream, OVERRIDE_PROCESS)