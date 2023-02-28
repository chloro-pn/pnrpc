#pragma once

#include <string>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

#define PNRPC_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__);
#define PNRPC_LOG_INFO(...) spdlog::info(__VA_ARGS__);
#define PNRPC_LOG_WARN(...) spdlog::warn(__VA_ARGS__);
#define PNRPC_LOG_ERROR(...) spdlog::error(__VA_ARGS__);