load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository', 'new_git_repository')

git_repository(
  name = "bridge",
  remote = "https://github.com/chloro-pn/bridge",
  commit = "21e8c187dc67b73145a7bab462f0a39773325f57",
)

git_repository(
    name = "googletest",
    remote = "https://ghproxy.com/https://github.com/google/googletest",
    tag = "release-1.11.0",
)

new_git_repository(
    name = "async_simple",
    remote = "https://github.com/alibaba/async_simple",
    commit = "04885f0a59f9a848a67d43d6be2bb9ad2e32d739",
    build_file = "//third_party:async_simple.build",
)