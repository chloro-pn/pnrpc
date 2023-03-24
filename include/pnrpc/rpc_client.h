#pragma once

#include <cstdint>
#include <string>
#include <optional>

#include "pnrpc/log.h"
#include "pnrpc/stream.h"
#include "pnrpc/rpc_concept.h"
#include "pnrpc/util.h"
#include "pnrpc/rpc_ret_code.h"

#include "asio.hpp"

namespace pnrpc {


template <typename RequestType, typename ResponseType, uint32_t pcode> requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcStubBase {
 public:
   using request_t = RequestType;
   using response_t = ResponseType;

   RpcStubBase(asio::io_context& io, const std::string& ip, uint16_t port) : io_(io),
                                                                             socket_(io_),
                                                                             ip_(ip),
                                                                             port_(port),
                                                                             request_stream(pcode),
                                                                             response_stream() {
    request_stream.update_bind_socket(&socket_);
    response_stream.update_bind_socket(&socket_);
   }

  asio::awaitable<asio::ip::tcp::endpoint> async_connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    co_return co_await asio::async_connect(socket_, ep, asio::use_awaitable);
  }

  asio::ip::tcp::endpoint connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    return asio::connect(socket_, ep);
  }

 private:
  asio::io_context& io_;
  asio::ip::tcp::socket socket_;
  std::string ip_;
  uint16_t port_;

 protected:
  ClientToServerStream<request_t> request_stream;
  ServerToClientStream<response_t> response_stream;
};

template <typename RequestType, typename ResponseType, uint32_t pcode, RpcType rpc_type>
class RpcStub : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;

  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port) {}

  asio::awaitable<int> rpc_call_coro(const request_t& r, response_t& response) {
    co_await this->request_stream.Send(r, true);
    uint32_t ret_code = 0;
    std::string err_msg;
    auto tmp = co_await this->response_stream.Read(ret_code, err_msg);
    if (ret_code != RPC_OK) {
      PNRPC_LOG_INFO("rpc call failed, ret_code = {}, err_msg = {}", ret_code, err_msg);
    }
    if (!tmp.has_value()) {
      PNRPC_LOG_WARN("rpc {} no response", pcode);
    } else {
      response = std::move(tmp).value();
    }
    co_return ret_code;
  }
};

template <typename RequestType, typename ResponseType, uint32_t pcode>
class RpcStub<RequestType, ResponseType, pcode, RpcType::ClientSideStream> : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;

  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port), send_eof_(false), recved_(false) {}

  asio::awaitable<int> send_request(const request_t& request, bool eof = false) {
    if (send_eof_ == true) {
      co_return RPC_SEND_AFTER_EOF;
    }
    co_await this->request_stream.Send(request, eof);
    send_eof_ = eof;
    co_return RPC_OK;
  }

  asio::awaitable<int> recv_response(response_t& response) {
    if (send_eof_ == false) {
      co_return  RPC_RECV_BEFORE_EOF;
    }
    if (recved_ == true) {
      co_return RPC_RECV_DUPLICATE;
    }
    uint32_t ret_code = 0;
    std::string err_msg;
    auto tmp = co_await this->response_stream.Read(ret_code, err_msg);
    recved_ = true;
    if (ret_code != RPC_OK) {
      PNRPC_LOG_WARN("rpc response error : {}, {}", ret_code, err_msg);
    }
    if (!tmp.has_value()) {
      PNRPC_LOG_WARN("rpc no response : {}", ret_code);
    }
    response = std::move(tmp).value();
    co_return ret_code;
  }

 private:
  bool send_eof_;
  bool recved_;
};

template <typename RequestType, typename ResponseType, uint32_t pcode>
class RpcStub<RequestType, ResponseType, pcode, RpcType::ServerSideStream> : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;
  
  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port), send_eof_(false) {}

  asio::awaitable<int> send_request(const request_t& request) {
    if (send_eof_ == true) {
      co_return RPC_SEND_AFTER_EOF;
    }
    send_eof_ = true;
    co_await this->request_stream.Send(request, send_eof_);
    co_return RPC_OK;
  }

  asio::awaitable<int> recv_response(std::optional<response_t>& response) {
    if (send_eof_ == false) {
      co_return  RPC_RECV_BEFORE_EOF;
    }
    uint32_t ret_code = 0;
    std::string err_msg;
    response = co_await this->response_stream.Read(ret_code, err_msg);
    if (ret_code != RPC_OK) {
      PNRPC_LOG_WARN("rpc response error : {}, {}", ret_code, err_msg);
    }
    co_return ret_code;
  }

 private:
  bool send_eof_;
};

template <typename RequestType, typename ResponseType, uint32_t pcode>
class RpcStub<RequestType, ResponseType, pcode, RpcType::BidirectStream> : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;

  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port), send_eof_(false) {}

  asio::awaitable<int> send_request(const request_t& request, bool eof = false) {
    if (send_eof_ == true) {
      co_return RPC_SEND_AFTER_EOF;
    }
    send_eof_ = eof;
    co_await this->request_stream.Send(request, eof);
    co_return RPC_OK;
  }

  asio::awaitable<int> recv_response(std::optional<response_t>& response) {
    if (send_eof_ == false) {
      co_return  RPC_RECV_BEFORE_EOF;
    }
    uint32_t ret_code = 0;
    std::string err_msg;
    response = co_await this->response_stream.Read(ret_code, err_msg);
    if (ret_code != RPC_OK) {
      PNRPC_LOG_WARN("rpc response error : {}, {}", ret_code, err_msg);
    }
    co_return ret_code;
  }

 private:
  bool send_eof_;
};

}