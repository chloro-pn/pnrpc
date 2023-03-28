#include "gtest/gtest.h"

#include "boost/mysql.hpp"
#include "boost/asio.hpp"

TEST(mysql, all) {
  boost::asio::io_context ctx;
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

  boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);

  boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
  auto eps = resolver.resolve("localhost", "3306");

  boost::mysql::handshake_params params("pn", "123456", "test");
  conn.connect(*eps.begin(), params);

  const char* sql = "select 'Hello World!'";
  boost::mysql::results result;
  conn.query(sql, result);
  EXPECT_EQ("Hello World!", result.rows().at(0).at(0).as_string());
  conn.close();
}