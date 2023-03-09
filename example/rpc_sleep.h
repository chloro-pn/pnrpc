#pragma once

#include "pnrpc/net_server.h"

#include <memory>

extern std::unique_ptr<asio::io_context> global_default_ctx;

RPC_DECLARE(Sleep, uint32_t, uint32_t, 0x02, OVERRIDE_PROCESS OVERRIDE_BIND)