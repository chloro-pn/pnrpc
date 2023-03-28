#pragma once

#ifdef PNRPC_USE_BOOST
#include "boost/asio.hpp"
#include "boost/asio/experimental/awaitable_operators.hpp"
#include "boost/system/system_error.hpp"

#else
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#endif

namespace pnrpc {

#ifdef PNRPC_USE_BOOST

namespace net = ::boost::asio;
using system_error = ::boost::system::system_error;

#else 

namespace net = ::asio;
using system_error = ::asio::system_error;

#endif

}