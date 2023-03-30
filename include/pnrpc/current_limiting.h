#pragma once

#include <chrono>

using namespace std;
using namespace std::chrono;

namespace pnrpc {

class CurrentLimiting {
 public:
  CurrentLimiting(size_t uwl = 0) : start_time_(system_clock::now()), up_water_level_(uwl) {

  }

  void update_up_water_level(size_t uwl) {
    up_water_level_ = uwl;
  }

  size_t calcualte_sleep_time(size_t total_bytes) {
    if (up_water_level_ == 0) {
      return 0;
    }
    auto now = system_clock::now();
    double seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    double max_bytes = seconds * up_water_level_;
    // 未达到限流条件
    if (total_bytes <= max_bytes) {
      return 0;
    }
    return static_cast<size_t>((total_bytes - max_bytes) / (double)up_water_level_);
  }

 private:
  time_point<system_clock> start_time_;
  size_t up_water_level_; // 单位 字节 / 秒
};

}