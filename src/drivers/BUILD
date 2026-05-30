load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "pl011",
    srcs = glob([
        "pl011/*.c",
    ]),
    hdrs = glob(["pl011/**/*.h"]),
    copts = [
        "-Wall",
    ],
    deps = [
        "//:common_headers",
    ],
    includes = ["//:include/"],
    alwayslink = True,
)

cc_library(
    name = "gic",
    srcs = glob([
        "gic/*.c",
        "gic/*.cc",
    ]),
    hdrs = glob(["gic/*.h"]),
    includes = ["//:include/"],
    copts = [
        "-Wall",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-threadsafe-statics",
    ],
    deps = [
       "//:common_headers",
       "//:aarch64_as"
    ],
    alwayslink = True,
)