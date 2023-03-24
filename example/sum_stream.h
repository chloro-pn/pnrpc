#pragma once

#include "pnrpc/rpc_declare.h"

RPC_DECLARE(SumStream, uint32_t, uint32_t, 0x04, pnrpc::RpcType::ClientSideStream, OVERRIDE_PROCESS)