package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pnrpc",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/*.cc"]),
  includes = ["include"],
  deps = [
    "@bridge//:bridge",
    "@spdlog//:spdlog",
    "@token_bucket//:token_bucket",
  ] + select(
    {
      "//bazel_config:pnrpc_use_boost_asio" : ["@boost//:asio"],
      "//conditions:default" : ["@asio//:asio"],
    }
  ),
  defines = select(
    {
      "//bazel_config:pnrpc_use_boost_asio" : ["PNRPC_USE_BOOST"],
      "//conditions:default" : [],
    }
  ),
)

cc_binary(
  name = "example",
  srcs = glob(["example/*.cc", "example/*.h"]),
  deps = [
    ":pnrpc",
  ],
  includes = ["example"]
)

cc_binary(
  name = "redis_client",
  srcs = select(
    {
      "//bazel_config:pnrpc_use_boost_redis" : glob(["example/redis/*.cc", "example/redis/*.h"], ["example/redis/main.cc"]),
      "//conditions:default" : ["example/redis/main.cc"],
    }
  ),
  deps = select(
    {
      "//bazel_config:pnrpc_use_boost_redis" : ["@redis//:redis"],
      "//conditions:default" : [],
    }
  ),
  includes = ["example/redis"],
)

cc_test(
  name = "test",
  srcs = glob(["test/*.cc", "test/*.h"], exclude = ["test/mysql_test.cc"]) + select(
    {
      "//bazel_config:pnrpc_use_boost_mysql" : ["test/mysql_test.cc"],
      "//conditions:default" : [],
    }
  ),
  includes = ["test"],
  deps = [
    "@bridge//:bridge",
    "@googletest//:gtest",
    "@googletest//:gtest_main",
  ] + select(
    {
      "//bazel_config:pnrpc_use_boost_mysql" : ["@mysql//:mysql"],
      "//conditions:default" : [],
    }
  )
)