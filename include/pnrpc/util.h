#pragma once
#include <string>
#include <cstring>
#include <stdexcept>
#include <chrono>

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
void integralSeri(T t, std::string& appender) {
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    t = flipByByte(t);
  }
  size_t old_size = appender.size();
  appender.resize(old_size + sizeof(t));
  memcpy(&appender[old_size], &t, sizeof(t));
}

// 从 [ptr, ptr + buf_len)的buffer中按照大端格式解析出类型为T的变量，由调用者保证buf_len >= sizeof(T).
template <typename T>
T integralParse(const char* ptr, size_t buf_len) {
  if (buf_len < sizeof(T)) {
    throw std::logic_error("invalid integralParse request");
  }
  T ret;
  memcpy(&ret, ptr, sizeof(T));
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    ret = flipByByte(ret);
  }
  return ret;
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