load("//tools/workspace:default.bzl", "add_default_repositories")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

add_default_repositories()

load("@rpi_bazel//tools/workspace:default.bzl", rpi_bazel_add = "add_default_repositories")

rpi_bazel_add()

http_archive(
  name = "abyz_me_uk_pigpio",
  urls = ["http://abyz.me.uk/rpi/pigpio/pigpio.zip"],
  sha256 = "9b58a920f2746303123f6d47810d4c80dda035478f3bbb39a7061166530867d5",
  build_file = "@//:abyz_me_uk_pigpio.BUILD",
)


http_archive(
    name = "googletest",
    url = "https://github.com/google/googletest/archive/release-1.10.0.tar.gz",
    sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    strip_prefix = "googletest-release-1.10.0",
)
