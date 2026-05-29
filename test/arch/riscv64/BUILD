load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "assembly",
    srcs = glob([
        "**/*.S",
        "**/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-D__ASSEMBLY__",
        "-Wall",
        "-mcmodel=medany",
    ],
    deps = [

    ],
    includes = ["include/"],
    alwayslink = True,
)

cc_library(
    name = "guest-core",
    srcs = glob(
        ["**/*.c",
        "**/*.h"],

    ) ,
    hdrs = glob(["include/**/*.h"]) ,
    copts = [
        "-Wall",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
        "-mcmodel=medany"
    ],
    includes = [
        "include/",
    ],
    deps = [
        ":assembly",
        "//:common_headers",
        "//:utils",
    ],
    alwayslink = True,

)

cc_binary(
    name = "guest-elf",

    linkopts = [
        "-nostdlib",
        "-nostartfiles",
        "-nodefaultlibs",
    ] + [
        "-Wl,-T$(location :linker.lds)",
        "-Wl,--build-id=none",
    ],
    deps = [
        ":guest-core",
        ":linker.lds",
    ],
)

genrule(
    name = "guest-bin",
    srcs = [":guest-elf"],
    outs = ["guest.bin"],
    cmd = "$(OBJCOPY) -O binary $(location :guest-elf) $@",
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)