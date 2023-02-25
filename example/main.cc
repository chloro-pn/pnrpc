#include "pnrpc/rpc_define.h"
#include "pnrpc/rpc_server.h"
#include <cstring>
#include <iostream>


int main() {
  REGISTER_RPC(add, 0x00)
  REGISTER_RPC(num_add, 0x01)

  RPCaddSTUB client;
  auto request = std::make_unique<RPC_add_request_type>();
  request->str = "hello world";
  client.set_request(std::move(request));
  auto msg = client.create_request_message();
  //
  // ... network
  // 
  size_t pcode = static_cast<size_t>(*reinterpret_cast<uint32_t*>(&msg[0]));
  auto processor = RpcServer::Instance().GetProcessor(pcode);
  if (processor == nullptr) {
    std::cerr << "invalid rpc request, not find " << pcode << std::endl;
    return -1;
  }
  // parse request message
  processor->create_request_from_raw_bytes(&msg[sizeof(uint32_t)], msg.size() - sizeof(uint32_t));
  processor->process();
  auto ret = processor->create_response_to_raw_btes();
  // 
  // ... network
  //
  const std::string& result = client.create_response(&ret[0], ret.size());
  std::cout << result << std::endl;

  RPCnum_addSTUB c2;
  c2.set_request(std::make_unique<uint32_t>(2));
  msg = c2.create_request_message();
  pcode = static_cast<size_t>(*reinterpret_cast<uint32_t*>(&msg[0]));
  processor = RpcServer::Instance().GetProcessor(pcode);
  processor->create_request_from_raw_bytes(&msg[sizeof(uint32_t)], msg.size() - sizeof(uint32_t));
  processor->process();
  ret = processor->create_response_to_raw_btes();
  uint32_t result2 = c2.create_response(&ret[0], ret.size());
  std::cout << result2 << std::endl;
  return 0;
} 