#include "mysql_request.h"
#include "boost/mysql.hpp"

pnrpc::net::awaitable<void> RPCMysqlRequest::process() {
  auto request = (co_await get_request_arg()).value();
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

  boost::mysql::tcp_ssl_connection conn(co_await pnrpc::net::this_coro::executor, ssl_ctx);

  boost::asio::ip::tcp::resolver resolver(co_await pnrpc::net::this_coro::executor);
  auto eps = co_await resolver.async_resolve("localhost", "3306", boost::asio::use_awaitable);

  boost::mysql::handshake_params params(request.user_name, request.password, request.db_name);
  co_await conn.async_connect(*eps.begin(), params, boost::asio::use_awaitable);

  const char* sql = "select 'Hello World!'";
  boost::mysql::results result;
  co_await conn.async_query(sql, result, boost::asio::use_awaitable);
  co_await set_response_arg(result.rows().at(0).at(0).as_string(), true);
  co_await conn.async_close(boost::asio::use_awaitable);
}