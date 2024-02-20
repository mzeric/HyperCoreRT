#!/usr/bin/env bash

QEMU=qemu-system-aarch64
QEMU=/mnt/sdb1/xtx/work/qemu/build/qemu-system-aarch64
${QEMU} \
   -M virt,virtualization=true,type=virt,gic-version=3,iommu="smmuv3" \
   -cpu max  -nographic -smp 4 -m 4000 \
   -no-reboot \
   -kernel ./bazel-bin/hyper-elf \
   -device loader,file=./bazel-bin/hyper.dtb,force-raw=on,addr=0x42000000 \
   -monitor telnet:127.0.0.1:4444,server,nowait\
