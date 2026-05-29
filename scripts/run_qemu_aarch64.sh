#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PARENT_DIR="$(dirname "$PROJECT_DIR")"

# --- Configuration ---
OUT_DIR="${PROJECT_DIR}/output"
IMAGE="${IMAGE:-${PARENT_DIR}/Image}"
LINUX_DTB="${LINUX_DTB:-${PARENT_DIR}/linux.dtb}"
ROOTFS="${ROOTFS:-${PARENT_DIR}/rootfs.img}"
SMP="${SMP:-1}"
MEM="${MEM:-6000}"

QEMU="qemu-system-aarch64"

# --- Check artifacts ---
if [ ! -f "${OUT_DIR}/core.bin" ]; then
    echo "Error: ${OUT_DIR}/core.bin not found. Run 'make' or './scripts/build_output.sh' first."
    exit 1
fi

if [ ! -f "${OUT_DIR}/hyper.dtb" ]; then
    echo "Error: ${OUT_DIR}/hyper.dtb not found. Run 'make' or './scripts/build_output.sh' first."
    exit 1
fi

if [ ! -f "${IMAGE}" ]; then
    echo "Error: Guest kernel image not found at ${IMAGE}"
    echo "Set IMAGE=<path> to specify."
    exit 1
fi

# --- Build QEMU command ---
QEMU_CMD="${QEMU} \
    -M virt,virtualization=true,gic-version=3,secure=on \
    -cpu cortex-a57 \
    -nographic \
    -smp ${SMP} \
    -m ${MEM} \
    -no-reboot \
    -kernel ${OUT_DIR}/core.bin \
    -dtb ${OUT_DIR}/hyper.dtb \
    -device loader,file=${IMAGE},force-raw=on,addr=0x50200000"

if [ -f "${LINUX_DTB}" ]; then
    QEMU_CMD="${QEMU_CMD} \
    -device loader,file=${LINUX_DTB},force-raw=on,addr=0x65000000"
fi

if [ -f "${ROOTFS}" ]; then
    QEMU_CMD="${QEMU_CMD} \
    -device loader,file=${ROOTFS},force-raw=on,addr=0x66000000"
fi

echo "Launching QEMU..."
echo "  kernel:    ${OUT_DIR}/core.bin"
echo "  dtb:       ${OUT_DIR}/hyper.dtb"
echo "  image:     ${IMAGE}"
[ -f "${LINUX_DTB}" ] && echo "  linux-dtb: ${LINUX_DTB}"
[ -f "${ROOTFS}" ] && echo "  rootfs:    ${ROOTFS}"
echo ""

exec ${QEMU_CMD}
