#pragma once

#include <concepts>
#include <cstdint>
#include <string>

namespace pnrpc {

template <typename T>
concept IntegralType = std::same_as<T, uint8_t> || std::same_as<T, int8_t> ||
                       std::same_as<T, uint16_t> || std::same_as<T, int16_t> ||
                       std::same_as<T, uint32_t> || std::same_as<T, int32_t> ||
                       std::same_as<T, uint64_t> || std::same_as<T, int64_t>;

template <typename T>
concept RpcBasicType = IntegralType<T> || std::same_as<std::string, T>;

template <typename T>
concept RpcTypeConcept = requires (T t, const char* ptr, size_t len, std::string& appender) {
  { T::create_from_raw_bytes(ptr, len) } -> std::same_as<T>;
  { T::to_raw_bytes(t, appender) } -> std::same_as<void>;
} || RpcBasicType<T>;

}