#pragma once

#include <concepts>
#include <cstdint>
#include <string>

namespace pnrpc {

template <typename T>
concept RpcBasicType = std::same_as<uint32_t, T> || std::same_as<std::string, T>;

template <typename T>
concept RpcTypeConcept = requires (T t, const char* ptr, size_t len, std::string& appender) {
  { T::create_from_raw_bytes(ptr, len) } -> std::same_as<T>;
  { T::to_raw_bytes(t, appender) } -> std::same_as<void>;
} || RpcBasicType<T>;

}