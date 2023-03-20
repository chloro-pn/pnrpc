#pragma once

#include "pnrpc/net_server.h"

RPC_DECLARE(SumStream, uint32_t, uint32_t, 0x04, pnrpc::RpcType::ClientSideStream, OVERRIDE_PROCESS)