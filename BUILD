load("@rules_cc//cc:defs.bzl", "cc_library")
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "start_head",
    srcs = select({
        "@platforms//cpu:aarch64": glob(
            [
                "src/arch/aarch64/**/*.c",
                "src/arch/aarch64/**/*.S",
                "src/arch/aarch64/**/*.h",
            ],
        ),
        "@platforms//cpu:riscv64": glob(
            [
                "src/arch/riscv64/**/*.c",
                "src/arch/riscv64/**/*.S",
                "src/arch/riscv64/**/*.h",
            ],
        ),
    }),

    includes = ["include/"],
    hdrs = glob(["include/**/*.h"]),

    copts = [
        "-Wall",
        # "-fpic",
    ],
    alwayslink = True,
)

[cc_library(
    name = "%s_file" % f,
    srcs = [f],
    visibility = ["//visibility:public"],
) for f in [
    "aarch64",
    "riscv64",
]]

cc_library(
    name = "utils",
    srcs = glob(["src/utils/**/*.c"]),
    copts = [
        "-Wall",
    ],
    linkopts = [
        "-lc",  # need newlib here
        "-lgcc",
    ],
    alwayslink = True,
)

filegroup(
    name = "linker_script",
    srcs = [
        "src/arch/aarch64/linker.ld",
    ],
)

cc_library(
    name = "hyper-core",
    srcs = glob(
        ["src/core/**/*.c"],
        ["src/core/**/*.h"],
    ) + ["src/main.c"],
    copts = [
        "-Wall",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
    ],
    includes = [
        "include/",
    ],
    hdrs = glob(["include/**/*.h"]),

)

cc_binary(
    name = "hyper-elf",
    additional_linker_inputs = select({
        "@platforms//cpu:aarch64": glob(["src/arch/aarch64/*.ld"]),
        "@platforms//cpu:riscv64": glob(["src/arch/riscv64/*.ld"]),
    }),
    linkopts = [
        "-nostdlib",
        "-nostartfiles",
        "-nodefaultlibs",
        "-static",
    ] + select({
        "@platforms//cpu:aarch64": ["-Wl,--script=$(location :src/arch/aarch64/linker.ld)"],
        "@platforms//cpu:riscv64": ["-Wl,--script=$(location :src/arch/riscv64/linker.ld)"],
        # "-Wl,--script=$(location linker_script)"
    }),
    deps = [
        ":start_head",
        ":utils",
        ":hyper-core",
    ],
)

genrule(
    name = "bin",
    srcs = [":hyper-elf"],
    outs = ["core.bin"],
    cmd = "$(OBJCOPY) -O binary $(location :hyper-elf) $@",
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)

config_setting(
    name = "arm_cpu",
    values = {"cpu": "arm"},
)

config_setting(
    name = "x86_cpu",
    values = {"cpu": "x86_64"},
)

config_setting(
    name = "riscv_cpu",
    values = {"cpu": "riscv"},
)

platform(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

platform(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:aarch64",
    ],
)

platform(
    name = "linux_riscv64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:riscv64",
    ],
)
