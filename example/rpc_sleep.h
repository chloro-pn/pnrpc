#pragma once

#include <memory>

#include "pnrpc/rpc_declare.h"

extern std::unique_ptr<pnrpc::net::io_context> global_default_ctx;

RPC_DECLARE(Sleep, uint32_t, uint32_t, 0x02, pnrpc::RpcType::Simple, OVERRIDE_PROCESS OVERRIDE_BIND)