
#include "download.h"

asio::awaitable<void> RPCDownload::process() {
  auto filename = co_await get_request_arg();
  co_await set_response_arg(std::make_unique<std::string>("hello"), false);
  co_await set_response_arg(std::make_unique<std::string>("world"), true);
  co_return;
}