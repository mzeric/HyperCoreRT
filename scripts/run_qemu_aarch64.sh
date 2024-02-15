#!/usr/bin/env bash

QEMU=qemu-system-aarch64
${QEMU} \
   -M virt,virtualization=true,type=virt,gic-version=3 \
   -cpu cortex-a57 -nographic -smp 4 -m 4000 \
   -no-reboot \
   -kernel ./bazel-bin/hyper-elf \
   -monitor telnet:127.0.0.1:4444,server,nowait\
