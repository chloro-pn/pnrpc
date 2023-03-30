/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_EXAMPLES_COMMON_HPP
#define BOOST_REDIS_EXAMPLES_COMMON_HPP

#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

auto connect(std::shared_ptr<connection> conn, std::string const& host, std::string const& port)
    -> boost::asio::awaitable<void>;

auto start(boost::asio::awaitable<void> op) -> int;

#endif  // BOOST_REDIS_EXAMPLES_COMMON_HPP