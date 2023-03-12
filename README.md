# pnrpc
pnrpc is an RPC framework based on asio c++20 coroutines

## features
* 基于asio c++20 coroutines实现，因此可以无缝支持asio提供的协程基础设施（co_spawn、dispatch、Error Handling、Co-ordinating Parallel Coroutines等等）；
* 可以将异步操作封装为awaiter并以同步的方式书写代码，例如example/async；
* 提供了rpc client的协程实现，这意味着你可以在rpc实现函数（是一个coroutine）中发起对其他rpc的请求，而这些都是协程化非阻塞的，例如example/async；
* 支持多种线程模型：
    * **单线程模型**：这种模型下accept、每个socket的网络io以及rpc调用都在一个线程中进行；
    * **accept线程+多个io线程模型**：这种模型下accept单独占一个线程，将接收到的socket分配给多个io线程，每个io线程负责多个socket的网络io以及rpc调用；
    * **accpet线程+多个io线程+多个rpc调用线程模型**：这种模型和第二种模型的区别在于，io线程只负责socket的网络io，用户可以通过将rpc接口绑定到不同的自定义io_context，这些自定义io_context将负责rpc调用逻辑。（可以将多个rpc接口绑定到一个io_context上，也可以将一个rpc接口绑定到多个io_context上，这是十分自由的）
* 支持为rpc接口绑定限流策略，例如在example/sum这个接口的实现中绑定了令牌桶的限流策略。


## requirement
* cpp compiler supporting c++20 (like g++-11)
* bazel 6.0.0

## example

echo.h:
```c++
#pragma once

#include "pnrpc/net_server.h"

RPC_DECLARE(Echo, std::string, std::string, 0x01, OVERRIDE_PROCESS)
```

echo.cc:
```c++
#include "echo.h"

asio::awaitable<void> RPCEcho::process() {
  response.reset(new std::string(*request));
  co_return;
}
```

server:
```c++
#include "pnrpc/net_server.h"

#include "echo.h"

int main() {
  REGISTER_RPC(Echo, 0x01)

  NetServer ns("127.0.0.1", 44444, 4);
  ns.run();
  return 0;
}
```

client:
```c++
#include "pnrpc/net_server.h"

#include "echo.h"

#include <iostream>

int main() {
  // rpc client支持两种访问模式，同步阻塞式访问和协程式访问，同步阻塞式不需要运行io_context，虽然构造的时候需要指定。
  asio::io_context io;

  RPCEchoSTUB echo_client(io, "127.0.0.1", 44444);
  echo_client.connect();
  auto r2 = std::make_unique<std::string>("hello world");
  auto response2 = std::make_unique<std::string>();
  ret_code = echo_client.rpc_call(std::move(r2), response2);
  std::cout << *response2 << std::endl;
  return 0;
}
```