load("@rules_cc//cc:defs.bzl", "cc_library")
package(default_visibility = ["//visibility:public"])

cc_binary(
  name = "hyper-core",
  srcs =glob( ["src/**/**/*.c"]), 

  includes = [
    "include/**/*/*.h",
  ],

  additional_linker_inputs = [
  	":src/ld.script",
  	],

  deps = [
  ],

  copts = [
          "-Wno-unused-value",
          "-Wno-unused-function",
          "-Wno-unused-variable",
          "-ffreestanding",
  ],
  linkopts = [
      "-nostdlib",
      "-nostartfiles",
      "-nodefaultlibs",
      "-static",
      "-Wl,--script=$(location :src/ld.script)",
  ],
)

genrule(
	name = "bin",
	srcs = [":hyper-core"],
	outs = ["core.bin"],
	cmd = "$(OBJCOPY) -O binary $(location :hyper-core) $@",
        toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
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

