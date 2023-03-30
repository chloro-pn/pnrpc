#pragma once

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "pnrpc/asio_version.h"
#include "pnrpc/current_limiting.h"
#include "pnrpc/exception.h"
#include "pnrpc/log.h"
#include "pnrpc/packager.h"
#include "pnrpc/rpc_concept.h"
#include "pnrpc/rpc_type_creator.h"
#include "pnrpc/util.h"

namespace pnrpc {

/*
 * 类StreamBase封装了对socket上的数据流读写的操作，有如下特点：
 *  支持运行时切换socket；
 *  提供对流式数据的分包；
 *  记录在该socket上的一些统计数据（读写字节数、开始和结束的时间（todo））；
 *  支持同步阻塞式和协程式的接口；
 *  支持对读写操作的限流;
 */
class StreamBase {
 public:
  StreamBase() : socket_(nullptr), write_bytes_(0), read_bytes_(0) {}

  void update_bind_socket(net::ip::tcp::socket* s) { socket_ = s; }

  void update_read_limiting(size_t up_water) { read_limiting_.update_up_water_level(up_water); }

  void update_write_limiting(size_t up_water) { write_limiting_.update_up_water_level(up_water); }

 protected:
  net::awaitable<void> coro_send(const std::string& buf) {
    std::string tmp;
    uint32_t length = buf.size();
    if (length >= max_package_size) {
      throw PnrpcException("package is too large : " + std::to_string(length));
    }
    integralSeri(length, tmp);
    tmp.append(buf);
    size_t sleep_s = write_limiting_.calcualte_sleep_time(write_bytes_);
    if (sleep_s != 0) {
      net::steady_timer timer(co_await net::this_coro::executor, std::chrono::seconds(sleep_s));
      co_await timer.async_wait(net::use_awaitable);
    }
    co_await net::async_write(*socket_, net::buffer(tmp), net::use_awaitable);
    write_bytes_ += tmp.size();
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
    size_t sleep_s = write_limiting_.calcualte_sleep_time(write_bytes_);
    if (sleep_s != 0) {
      std::this_thread::sleep_for(std::chrono::seconds(sleep_s));
    }
    net::write(*socket_, net::buffer(tmp));
    write_bytes_ += tmp.size();
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
    size_t sleep_s = read_limiting_.calcualte_sleep_time(read_bytes_);
    if (sleep_s != 0) {
      net::steady_timer timer(co_await net::this_coro::executor, std::chrono::seconds(sleep_s));
      co_await timer.async_wait(net::use_awaitable);
    }
    co_await net::async_read(*socket_, net::buffer(buf), net::use_awaitable);
    read_bytes_ = read_bytes_ + sizeof(uint32_t) + length;
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
    size_t sleep_s = read_limiting_.calcualte_sleep_time(read_bytes_);
    if (sleep_s != 0) {
      std::this_thread::sleep_for(std::chrono::seconds(sleep_s));
    }
    net::read(*socket_, net::buffer(buf));
    read_bytes_ = read_bytes_ + sizeof(uint32_t) + length;
    return buf;
  }

 private:
  net::ip::tcp::socket* socket_;
  size_t write_bytes_;
  size_t read_bytes_;
  CurrentLimiting read_limiting_;
  CurrentLimiting write_limiting_;
};

template <typename RpcType>
requires RpcTypeConcept<RpcType> || std::is_void<RpcType>::value class ClientToServerStream : public StreamBase {
 public:
  explicit ClientToServerStream(uint32_t pcode)
      : StreamBase(), set_init_package_(false), pcode_(pcode), read_eof_(false), send_eof_(false) {}

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

  uint32_t get_pcode() const { return pcode_; }

  bool get_eof() const { return read_eof_; }

  void set_init_package(RpcType&& package, bool eof) {
    pkg_ = std::move(package);
    set_init_package_ = true;
    read_eof_ = eof;
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
  explicit ClientToServerStream() : StreamBase(), pcode_(0), eof_(false) {}

  net::awaitable<std::string_view> Read(std::string& buf) {
    buf = co_await coro_recv();
    RequestPackager<void> rp;
    co_return rp.parse_request_package(buf, pcode_, eof_);
  }

  uint32_t get_pcode() const { return pcode_; }

  bool get_eof() const { return eof_; }

 private:
  uint32_t pcode_;
  bool eof_;
};

template <typename RpcType>
requires RpcTypeConcept<RpcType>
class ServerToClientStream : public StreamBase {
 public:
  explicit ServerToClientStream() : StreamBase(), read_eof_(false), send_eof_(false) {}

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

 private:
  net::ip::tcp::socket* socket_;
  bool read_eof_;
  bool send_eof_;
};

class ErrorStream : public StreamBase {
 public:
  explicit ErrorStream() : StreamBase() {}

  net::awaitable<void> SendErrorMsg(const std::string& err_msg, uint32_t ret_code) {
    std::string buf;
    ResponsePackager<void> rp;
    rp.seri_error_package(err_msg, ret_code, buf);
    co_await coro_send(buf);
    co_return;
  }
};
}  // namespace pnrpc