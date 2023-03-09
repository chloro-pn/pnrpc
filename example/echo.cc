#include "echo.h"

asio::awaitable<void> RPCEcho::process() {
  response.reset(new std::string(*request));
  co_return;
}