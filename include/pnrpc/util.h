#pragma once

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

#include "pnrpc/exception.h"

namespace pnrpc {

enum class RpcType : uint8_t {
  Simple,
  ServerSideStream,
  ClientSideStream,
  BidirectStream,
};

// 16MB
constexpr size_t max_package_size = 16 * 1024 * 1024;

}  // namespace pnrpc

class Endian {
 public:
  enum class Type { Big, Little };

  static Endian& Instance() {
    static Endian obj;
    return obj;
  }

  Type GetEndianType() const { return type_; }

 private:
  Endian() {
    int i = 1;
    type_ = (*(char*)&i == 1) ? Type::Little : Type::Big;
  }

  Type type_;
};

template <typename T>
inline T flipByByte(T t) {
  T ret{0};
  char* ptr1 = reinterpret_cast<char*>(&ret);
  char* ptr2 = reinterpret_cast<char*>(&t);
  for (int i = 0; i < sizeof(T); ++i) {
    int j = sizeof(T) - 1 - i;
    ptr1[j] = ptr2[i];
  }
  return ret;
}

// 将T类型（必须是整型）的变量t按照大端格式序列化并添加到appender后面
template <typename T>
const char* integralSeri(T t, std::string& appender) {
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    t = flipByByte(t);
  }
  size_t old_size = appender.size();
  appender.resize(old_size + sizeof(t));
  memcpy(&appender[old_size], &t, sizeof(t));
  return &appender[old_size];
}

template <typename T>
inline void integralSeri(T t, char* ptr) {
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    t = flipByByte(t);
  }
  memcpy(ptr, &t, sizeof(t));
}

// 从 [ptr, ptr + buf_len)的buffer中按照大端格式解析出类型为T的变量，由调用者保证buf_len >= sizeof(T).
template <typename T>
T integralParse(const char* ptr, size_t buf_len) {
  if (buf_len < sizeof(T)) {
    throw pnrpc::PnrpcException("invalid integralParse request");
  }
  T ret;
  memcpy(&ret, ptr, sizeof(T));
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    ret = flipByByte(ret);
  }
  return ret;
}

template <typename T>
T integralParse(char (&buf)[sizeof(T)]) {
  return integralParse<T>(&buf[0], sizeof(T));
}

inline void pcodeSeri(uint32_t pcode, std::string& appender) { integralSeri<uint32_t>(pcode, appender); }

inline uint32_t ParsePcode(const char*& ptr, size_t& len) {
  auto pcode = integralParse<uint32_t>(ptr, len);
  ptr += sizeof(uint32_t);
  len -= sizeof(uint32_t);
  return pcode;
}

inline void eofSeri(bool eof, std::string& appender) {
  uint8_t flag = eof == 0;
  size_t old_size = appender.size();
  appender.resize(old_size + sizeof(flag));
  memcpy(&appender[old_size], &flag, sizeof(flag));
}

inline bool ParseEofFlag(const char*& ptr, size_t& len) {
  uint8_t eof = integralParse<uint8_t>(ptr, len);
  ptr += sizeof(uint8_t);
  len -= sizeof(uint8_t);
  return eof == 0;
}

class Timer {
 public:
  Timer() {}

  void Start() { start_ = std::chrono::system_clock::now(); }

  double End() {
    auto end = std::chrono::system_clock::now();
    auto use_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    return use_ms.count();
  }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_;
};