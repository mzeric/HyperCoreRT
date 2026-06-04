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
        "-std=gnu11",
    ] + select({
        "@platforms//cpu:riscv64": ["-march=rv64imac_zicsr", "-mabi=lp64"],
        "//conditions:default": [],
    }),
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
    ]),
    hdrs = glob(["gic/*.h"]),
    includes = ["//:include/"],
    copts = [
        "-Wall",
        "-std=gnu11",
    ] + select({
        "@platforms//cpu:riscv64": ["-march=rv64imac_zicsr", "-mabi=lp64"],
        "//conditions:default": [],
    }),
    deps = [
       "//:common_headers",
       "//:aarch64_as"
    ],
    alwayslink = True,
)