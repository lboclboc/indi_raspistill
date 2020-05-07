load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


def rpi_bazel_repository(name):
    commit = "35105e09fee4db0c252e861ebf97e43a2a06aef2"
    http_archive(
        name = name,
        url = "https://github.com/mjbots/rpi_bazel/archive/{}.zip".format(commit),
        sha256 = "a39c647513ce23c85259007970db90f824577b44731c46423e58871b946c1318",
        strip_prefix = "rpi_bazel-{}".format(commit),
    )
