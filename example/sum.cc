#include "sum.h"

asio::awaitable<void> RPCSum::process() {
  response = std::make_unique<uint32_t>(0.0);
  for(const auto& each : request->nums) {
    *response += each;
  }
  co_return;
}