#pragma once

#include "pnrpc/rpc_declare.h"

RPC_DECLARE(Echo, std::string, std::string, 0x01, pnrpc::RpcType::Simple, OVERRIDE_PROCESS)