#pragma once

#include <exception>
#include <string>

namespace pnrpc {

class PnrpcException : public std::exception {
 public:
  explicit PnrpcException(const std::string& w) : what_(w) {

  }

  const char* what() const noexcept override {
    if (what_.empty()) {
      return nullptr;
    }
    return &what_[0];
  }

 private:
  std::string what_;
};

}