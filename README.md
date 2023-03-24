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

## doc

##### rpc声明
首先需要通过宏```RPC_DECLARE```来声明rpc接口，以echo为例：
```c++
RPC_DECLARE(Echo, std::string, std::string, 0x01, pnrpc::RpcType::Simple, OVERRIDE_PROCESS)
```
参数的含义依次为:
* rpc接口名称（不可重复）
* 请求参数类型 
* 回复参数类型
  请求参数和回复参数类型需要满足如下concept，即除了内置基本类型，用户需要为参数类型指定序列化和反序列化函数：
  ```c++
  template <typename T>
  concept RpcTypeConcept = requires (T t, const char* ptr, size_t len, std::string& appender) {
    { T::create_from_raw_bytes(ptr, len) } -> std::same_as<T>;
    { T::to_raw_bytes(t, appender) } -> std::same_as<void>;
  } || RpcBasicType<T>;
  ```
* 编号（不可重复）
* rpc类型（一应一答型、客户端流式、服务器流式、双向流式四种类型），如下：
```c++
enum class RpcType : uint8_t {
  Simple,
  ServerSideStream,
  ClientSideStream,
  BidirectStream,
};
```
* 需要定制的功能，有如下选项可以指定（这些选项可以同时指定，使用空格分开即可）：
  * OVERRIDE_PROCESS 重载rpc处理函数，一般情况这个是必须指定的
  * OVERRIDE_BIND 重载网络绑定函数，用户可以通过重载这个函数将rpc接口的执行绑定到自定义的io_context上
  * OVERRIDE_RESTRICTOR 重载限流器函数，用户可以通过重载这个函数为rpc接口绑定限流器

##### rpc定义
声明之后，需要为rpc接口定制的功能提供定义，如果定制了OVERRIDE_PROCESS（参考example/echo的例子）：
```c++
asio::awaitable<void> RPCEcho::process() {
  auto request = co_await get_request_arg();
  co_await set_response_arg(request.value(), true);
  co_return;
}
```

如果定制了OVERRIDE_BIND（参考example/rpc_sleep的例子）：
```c++
std::unique_ptr<asio::io_context> global_default_ctx;

asio::io_context* RPCSleep::bind_io_context() {
  return global_default_ctx.get();
}
```

如果定制了OVERRIDE_RESTRICTOR（参考example/sum的例子）：
```c++
bool RPCSum::restrictor() {
  // 设置令牌桶的令牌入桶速率为100/s，容量为10000
  static TokenBucket tb(100, 100000);
  return tb.consume(1);
}
```

##### server端
在server端首先通过REGISTER_RPC宏注册rpc接口，然后定义NetServer类型变量并启动server，NetServer的构造函数参数分别是ip、port、io线程个数（如果不指定的话就是单线程模型，accept、每个socket的网络io以及rpc调用都在一个线程中进行）
```c++
  REGISTER_RPC(Sum)
  REGISTER_RPC(Echo)
  REGISTER_RPC(Sleep)
  REGISTER_RPC(Async)
  REGISTER_RPC(SumStream)
  REGISTER_RPC(Download)

  NetServer ns("127.0.0.1", 44444, 4);
  std::thread th([&]() {
    ns.run();
  });
```

##### client端
在声明rpc接口的时候，已经定义了客户端stub类，用户可以通过该类型的变量作为客户端访问对应的rpc，对于不同的rpc类型，客户端stub类提供了不同的接口，下面这个是ClientSideStream类型的例子，用户可以通过send_request函数发送流式数据（第二个参数为eof，设置为true时意味着流式数据传送完毕），然后通过recv_response函数接收回复信息。
```c++
  asio::io_context io;

  asio::co_spawn(io, [&io]() -> asio::awaitable<void> {
    RPCSumStreamSTUB sum_client(io, "127.0.0.1", 44444);
    co_await sum_client.async_connect();
    for(uint32_t x = 0; x < 3; ++x) {
      co_await sum_client.send_request(x);
    }
    co_await sum_client.send_request(3, true);
    uint32_t resp = 0;
    int ret_code = co_await sum_client.recv_response(resp);;
    assert(resp == 0 + 1 + 2 + 3);
    assert(ret_code == RPC_OK);
  }, asio::detached);
```
除了协程类型的stub接口外，还提供了阻塞式接口，如下：
```c++
  RPCEchoSTUB echo_client(io, "127.0.0.1", 44444);
  echo_client.connect();
  std::string resp;
  uint32_t ret_code = echo_client.rpc_call("helloworld", resp);
  assert(ret_code == RPC_OK);
  assert(resp == "helloworld");
```

## todo
* 提供多种客户端连接模型（短连接、连接池、socket复用）
* 完善支持的内置类型
* 补充单元测试和文档