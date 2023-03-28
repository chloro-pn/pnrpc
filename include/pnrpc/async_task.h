#pragma once

#include "pnrpc/asio_version.h"

namespace pnrpc {

template <typename ReturnType>
auto async_task(std::function<void(std::function<void(ReturnType)>&&)> request_task) {
  auto initiate = [request_task = std::move(request_task)]<typename Handler>(Handler&& handler) mutable {
    auto shared_handler = std::make_shared<Handler>(std::forward<Handler>(handler));
    auto&& resume_func = 
      [shared_handler](ReturnType rt) mutable -> void  {
        auto ex = net::get_associated_executor(*shared_handler);
        net::dispatch(ex, [shared_handler, rt = std::move(rt)]() mutable {
          (*shared_handler)(rt);
        });
      };
      request_task(std::move(resume_func));
  };
  auto h = net::use_awaitable_t<>();
  return net::async_initiate<net::use_awaitable_t<>, void(ReturnType)>(std::move(initiate), h);
}

}