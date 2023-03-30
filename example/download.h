#pragma once

#include <string>

#include "pnrpc/rpc_declare.h"

RPC_DECLARE(Download, std::string, std::string, 0x05, pnrpc::RpcType::ServerSideStream,
            OVERRIDE_PROCESS OVERRIDE_RESPONSE_LIMIT)