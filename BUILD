load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "hyper-core",
    srcs = select({
        "@platforms//cpu:x86_64": glob(
            ["src/arch/x86/**/**/*.c"],
            ["src/arch/x86/**/**/*.s"],
        ),
        "@platforms//cpu:aarch64": glob(
            ["src/arch/aarch64/**/**/*.c"],
            ["src/arch/aarch64/**/**/*.s"],
        ),
        "@platforms//cpu:riscv64": glob(
            ["src/arch/riscv/**/**/*.c"],
            ["src/arch/riscv/**/**/*.s"],
        ),
    }) + glob(
        ["src/**/**/*.c"],
        exclude = ["src/arch/"],
    ),
    additional_linker_inputs = [
        ":src/ld.script",
    ],
    copts = [
        "-Wno-unused-value",
        "-Wno-unused-function",
        "-Wno-unused-variable",
        "-ffreestanding",
    ],
    includes = [
        "include/**/*/*.h",
    ],
    linkopts = [
        "-nostdlib",
        "-nostartfiles",
        "-nodefaultlibs",
        "-static",
        "-Wl,--script=$(location :src/ld.script)",
    ],
    deps = [
    ],
)

genrule(
    name = "bin",
    srcs = [":hyper-core"],
    outs = ["core.bin"],
    cmd = "$(OBJCOPY) -O binary $(location :hyper-core) $@",
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
