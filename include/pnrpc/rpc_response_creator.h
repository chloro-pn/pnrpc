#pragma once

#include <vector>
#include <string>

template <typename ResponseType>
struct ResponseCreator {
  static std::vector<char> to_raw_bytes(const ResponseType& rt) {
    return ResponseType::to_raw_bytes(rt);
  }

  static std::unique_ptr<ResponseType> create_from_raw_bytes(const char* ptr, size_t len) {
    return ResponseType::create_from_raw_bytes(ptr, len);
  }
};

template <>
struct ResponseCreator<std::string> {
  static std::vector<char> to_raw_bytes(const std::string& rt) {
    std::vector<char> ret;
    for(auto& each : rt) {
      ret.push_back(each);
    }
    return ret;
  }

  static std::unique_ptr<std::string> create_from_raw_bytes(const char* ptr, size_t len) {
    return std::make_unique<std::string>(ptr, len);
  }
};

template <>
struct ResponseCreator<uint32_t> {
  static std::vector<char> to_raw_bytes(const uint32_t& rt) {
    std::vector<char> ret;
    ret.resize(sizeof(uint32_t));
    memcpy(&ret[0], &rt, sizeof(rt));
    return ret;
  }

  static std::unique_ptr<uint32_t> create_from_raw_bytes(const char* ptr, size_t len) {
    uint32_t tmp;
    memcpy(&tmp, ptr, len);
    return std::make_unique<uint32_t>(tmp);
  }
};