#pragma once

#include "pnrpc/rpc_concept.h"
#include "pnrpc/rpc_type_creator.h"
#include "pnrpc/util.h"
#include "pnrpc/packager.h"
#include "pnrpc/log.h"
#include "pnrpc/exception.h"
#include "pnrpc/asio_version.h"

#include <memory>
#include <cassert>
#include <string>
#include <string_view>
#include <optional>

namespace pnrpc {

// todo: 记录流上的读取、写入的字节数，记录收发包的开始结束时间等等统计信息
class StreamBase {
 public:
  StreamBase() : socket_(nullptr), write_bytes_(0), read_bytes_(0) {

  }

  void update_bind_socket(net::ip::tcp::socket* s) {
    socket_ = s;
  }

 protected:
  net::awaitable<void> coro_send(const std::string& buf) {
    std::string tmp;
    uint32_t length = buf.size();
    if (length >= max_package_size) {
      throw PnrpcException("package is too large : " + std::to_string(length));
    }
    integralSeri(length, tmp);
    tmp.append(buf);
    write_bytes_ += tmp.size();
    co_await net::async_write(*socket_, net::buffer(tmp), net::use_awaitable);
    co_return;
  }

  void send(const std::string& buf) {
    std::string tmp;
    uint32_t length = buf.size();
    if (length >= max_package_size) {
      throw PnrpcException("package is too large : " + std::to_string(length));
    }
    integralSeri(length, tmp);
    tmp.append(buf);
    write_bytes_ += tmp.size();
    net::write(*socket_, net::buffer(tmp));
  }

  net::awaitable<std::string> coro_recv() {
    char data[sizeof(uint32_t)];
    co_await net::async_read(*socket_, net::buffer(data), net::use_awaitable);
    auto length = integralParse<uint32_t>(data);
    if (length >= max_package_size) {
      throw PnrpcException("package is too large : " + std::to_string(length));
    }
    std::string buf;
    buf.resize(length);
    read_bytes_ = read_bytes_ + sizeof(uint32_t) + length;
    co_await net::async_read(*socket_, net::buffer(buf), net::use_awaitable);
    co_return buf;
  }

  std::string recv() {
    char data[sizeof(uint32_t)];
    net::read(*socket_, net::buffer(data));
    auto length = integralParse<uint32_t>(data);
    if (length >= max_package_size) {
      throw PnrpcException("package is too large : " + std::to_string(length));
    }
    std::string buf;
    buf.resize(length);
    read_bytes_ = read_bytes_ + sizeof(uint32_t) + length;
    net::read(*socket_, net::buffer(buf));
    return buf;
  }

 private:
  net::ip::tcp::socket* socket_;
  size_t write_bytes_;
  size_t read_bytes_;
};

template <typename RpcType> requires RpcTypeConcept<RpcType> || std::is_void<RpcType>::value
class ClientToServerStream : public StreamBase {
 public:
  explicit ClientToServerStream(uint32_t pcode) : StreamBase(), set_init_package_(false), pcode_(pcode), read_eof_(false), send_eof_(false) {

  }

  net::awaitable<void> Send(const RpcType& package, bool eof) {
    if (send_eof_ == true) {
      PNRPC_LOG_WARN("ClientToServerStream send package after send_eof");
      co_return;
    }
    send_eof_ = eof;
    std::string buf;
    RequestPackager<RpcType> rp;
    rp.seri_request_package(package, buf, pcode_, eof);
    co_await coro_send(buf);
    co_return;
  }

  void SendSync(const RpcType& package, bool eof) {
    if (send_eof_ == true) {
      PNRPC_LOG_WARN("ClientToServerStream send package after send_eof");
      return;
    }
    send_eof_ = eof;
    std::string buf;
    RequestPackager<RpcType> rp;
    rp.seri_request_package(package, buf, pcode_, eof);
    send(buf);
  }

  net::awaitable<std::optional<RpcType>> Read() {
    if (set_init_package_ == true) {
      set_init_package_ = false;
      co_return std::move(pkg_);
    }
    if (read_eof_ == true) {
      co_return std::optional<RpcType>();
    }
    std::string buf = co_await coro_recv();
    RequestPackager<RpcType> rp;
    uint32_t pc;
    auto pkg = rp.parse_request_package(buf, pc, read_eof_);
    assert(pc == pcode_);
    co_return pkg;
  }

  std::optional<RpcType> ReadSync() {
    if (set_init_package_ == true) {
      set_init_package_ = false;
      return std::move(pkg_);
    }
    if (read_eof_ == true) {
      return std::optional<RpcType>();
    }
    std::string buf = recv();
    RequestPackager<RpcType> rp;
    uint32_t pc;
    auto pkg = rp.parse_request_package(buf, pc, read_eof_);
    assert(pc == pcode_);
    return pkg;
  }

  uint32_t get_pcode() const {
    return pcode_;
  }

  bool get_eof() const {
    return read_eof_;
  }

  void set_init_package(RpcType&& package, bool eof) {
    pkg_ = std::move(package);
    set_init_package_ = true;
    read_eof_ = eof;
  }

  ~ClientToServerStream() {
    if (read_eof_ == false || send_eof_ == false) {
      PNRPC_LOG_WARN("ClientToServerStream error, read_eof = {}, send_eof = {}", read_eof_, send_eof_);
    }
  }

 private:
  RpcType pkg_;
  bool set_init_package_;
  uint32_t pcode_;
  bool read_eof_;
  bool send_eof_;
};

template <>
class ClientToServerStream<void> : public StreamBase {
 public:
  explicit ClientToServerStream() : StreamBase(), pcode_(0), eof_(false) {

  }

  net::awaitable<std::string_view> Read() {
    std::string buf = co_await coro_recv();
    RequestPackager<void> rp;
    co_return rp.parse_request_package(buf, pcode_, eof_);
  }

  uint32_t get_pcode() const {
    return pcode_;
  }

  bool get_eof() const {
    return eof_;
  }

 private:
  uint32_t pcode_;
  bool eof_;
};

template <typename RpcType> requires RpcTypeConcept<RpcType>
class ServerToClientStream : public StreamBase {
 public:
  explicit ServerToClientStream() : StreamBase(), read_eof_(false), send_eof_(false) {

  }

  net::awaitable<void> Send(const RpcType& package, uint32_t ret_code, bool eof) {
    if (send_eof_ == true) {
      PNRPC_LOG_WARN("ServerToClientStream send package after send_eof");
      co_return;
    }
    send_eof_ = eof;
    std::string buf;
    ResponsePackager<RpcType> rp;
    rp.seri_response_package(package, buf, ret_code, eof);
    co_await coro_send(buf);
    co_return;
  }

  void SendSync(const RpcType& package, uint32_t ret_code, bool eof) {
    if (send_eof_ == true) {
      PNRPC_LOG_WARN("ServerToClientStream send package after send_eof");
      return;
    }
    send_eof_ = eof;
    std::string buf;
    ResponsePackager<RpcType> rp;
    rp.seri_response_package(package, buf, ret_code, eof);
    send(buf);
  }

  net::awaitable<std::optional<RpcType>> Read(uint32_t& ret_code, std::string& err_msg) {
    if (read_eof_ == true) {
      co_return std::optional<RpcType>();
    }
    std::string buf = co_await coro_recv();
    ResponsePackager<RpcType> rp;
    auto pkg = rp.parse_response_package(buf);
    ret_code = pkg.ret_code;
    err_msg = pkg.err_msg;
    read_eof_ = pkg.eof;
    co_return std::move(pkg.response);
  }

  std::optional<RpcType> ReadSync(uint32_t ret_code, std::string& err_msg) {
    if (read_eof_ == true) {
      return std::optional<RpcType>();
    }
    std::string buf = recv();
    ResponsePackager<RpcType> rp;
    auto pkg = rp.parse_response_package(buf);
    ret_code = pkg.ret_code;
    err_msg = pkg.err_msg;
    read_eof_ = pkg.eof;
    return std::move(pkg.response);
  }

  ~ServerToClientStream() {
    if (read_eof_ == false || send_eof_ == false) {
      PNRPC_LOG_WARN("ServerToClientStream error, read_eof = {}, send_eof = {}", read_eof_, send_eof_);
    }
  }

 private:
  net::ip::tcp::socket* socket_;
  bool read_eof_;
  bool send_eof_;
};

class ErrorStream : public StreamBase {
 public:
  explicit ErrorStream() : StreamBase() {

  }

  net::awaitable<void> SendErrorMsg(const std::string& err_msg, uint32_t ret_code) {
    std::string buf;
    ResponsePackager<void> rp;
    rp.seri_error_package(err_msg, ret_code, buf);
    co_await coro_send(buf);
    co_return;
  }
};
}