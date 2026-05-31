#include "guest_dtb.h"
#include "arch_page.h"
#include "config.h"
#include "riscv_features.h"
#include "safe_printf.h"
#include <string.h>

#define FDT_MAGIC        0xd00dfeedu
#define FDT_VERSION      17u
#define FDT_LAST_COMPAT  16u
#define FDT_BEGIN_NODE   0x1u
#define FDT_END_NODE     0x2u
#define FDT_PROP         0x3u
#define FDT_END          0x9u
#define GUEST_DTB_SIZE   0x10000u
#define GUEST_RAM_BASE   0x90000000UL
#define GUEST_RAM_SIZE   0x01000000UL
#define GUEST_UART_BASE  0x20000000UL
#define GUEST_UART_SIZE  0x1000UL
#define GUEST_PLIC_BASE  0x0c000000UL
#define GUEST_PLIC_SIZE  0x400000UL

struct fdt_header_min {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
};

static u8 *g_dtb;
static u8 *g_struct;
static u8 *g_struct_start;
static char g_strings[2048];
static u32 g_strings_size;

static u32 bswap32(u32 v) {
    return ((v & 0xff000000u) >> 24) |
           ((v & 0x00ff0000u) >> 8) |
           ((v & 0x0000ff00u) << 8) |
           ((v & 0x000000ffu) << 24);
}

static u64 bswap64(u64 v) {
    return ((u64)bswap32((u32)v) << 32) | bswap32((u32)(v >> 32));
}

static void put_be32(void *p, u32 v) {
    *(u32 *)p = bswap32(v);
}

static void put_be64(void *p, u64 v) {
    *(u64 *)p = bswap64(v);
}

static void emit_be32(u32 v) {
    put_be32(g_struct, v);
    g_struct += sizeof(u32);
}

static void emit_bytes(const void *data, u32 len) {
    if (len) {
        memcpy(g_struct, data, len);
        g_struct += len;
    }
    while ((uintptr_t)g_struct & 3)
        *g_struct++ = 0;
}

static int string_offset(const char *name) {
    u32 off = 0;
    while (off < g_strings_size) {
        if (!strcmp(g_strings + off, name))
            return off;
        off += strlen(g_strings + off) + 1;
    }

    u32 len = strlen(name) + 1;
    if (g_strings_size + len > sizeof(g_strings))
        return -1;
    memcpy(g_strings + g_strings_size, name, len);
    off = g_strings_size;
    g_strings_size += len;
    return off;
}

static void begin_node(const char *name) {
    emit_be32(FDT_BEGIN_NODE);
    emit_bytes(name, strlen(name) + 1);
}

static void end_node(void) {
    emit_be32(FDT_END_NODE);
}

static void prop_raw(const char *name, const void *data, u32 len) {
    int off = string_offset(name);
    if (off < 0)
        return;

    emit_be32(FDT_PROP);
    emit_be32(len);
    emit_be32((u32)off);
    emit_bytes(data, len);
}

static void prop_empty(const char *name) {
    prop_raw(name, NULL, 0);
}

static void prop_string(const char *name, const char *value) {
    prop_raw(name, value, strlen(value) + 1);
}

static void prop_u32(const char *name, u32 value) {
    u32 be;
    put_be32(&be, value);
    prop_raw(name, &be, sizeof(be));
}

static void prop_u64_pair(const char *name, u64 a, u64 b) {
    u64 reg[2];
    put_be64(&reg[0], a);
    put_be64(&reg[1], b);
    prop_raw(name, reg, sizeof(reg));
}

static void prop_reg2(const char *name, u64 base, u64 size) {
    prop_u64_pair(name, base, size);
}

static void prop_interrupts_extended(u32 phandle, u32 irq) {
    u32 data[2];
    put_be32(&data[0], phandle);
    put_be32(&data[1], irq);
    prop_raw("interrupts-extended", data, sizeof(data));
}

void riscv_guest_dtb_init(void *host_fdt) {
    (void)host_fdt;

    g_dtb = (u8 *)phy_to_vir(RISCV_GUEST_DTB_ADDR);
    memset(g_dtb, 0, GUEST_DTB_SIZE);
    memset(g_strings, 0, sizeof(g_strings));
    g_strings_size = 0;

    fdt_header_min *hdr = (fdt_header_min *)g_dtb;
    u32 reserve_off = sizeof(*hdr);
    u32 struct_off = reserve_off + 16;
    g_struct_start = g_dtb + struct_off;
    g_struct = g_struct_start;

    begin_node("");
    prop_u32("#address-cells", 2);
    prop_u32("#size-cells", 2);
    prop_string("compatible", "riscv-virtio");
    prop_u32("interrupt-parent", 2);

    begin_node("chosen");
    prop_string("bootargs", "console=ttyS0,115200 earlycon=uart8250,mmio,0x20000000");
    prop_string("stdout-path", "/soc/serial@20000000:115200");
    end_node();

    begin_node("memory@90000000");
    prop_string("device_type", "memory");
    prop_reg2("reg", GUEST_RAM_BASE, GUEST_RAM_SIZE);
    end_node();

    begin_node("cpus");
    prop_u32("#address-cells", 1);
    prop_u32("#size-cells", 0);
    prop_u32("timebase-frequency", riscv_timebase_frequency());

    begin_node("cpu@0");
    prop_string("device_type", "cpu");
    prop_string("compatible", "riscv");
    prop_string("status", "okay");
    prop_string("riscv,isa", "rv64imac");
    prop_string("mmu-type", "riscv,sv48");
    prop_u32("reg", 0);
    begin_node("interrupt-controller");
    prop_empty("interrupt-controller");
    prop_string("compatible", "riscv,cpu-intc");
    prop_u32("#interrupt-cells", 1);
    prop_u32("phandle", 1);
    end_node();
    end_node();
    end_node();

    begin_node("soc");
    prop_u32("#address-cells", 2);
    prop_u32("#size-cells", 2);
    prop_string("compatible", "simple-bus");
    prop_empty("ranges");

    begin_node("serial@20000000");
    prop_string("compatible", "ns16550a");
    prop_reg2("reg", GUEST_UART_BASE, GUEST_UART_SIZE);
    prop_u32("clock-frequency", 3686400);
    prop_u32("current-speed", 115200);
    prop_u32("interrupt-parent", 2);
    prop_u32("interrupts", 10);
    end_node();

    begin_node("plic@c000000");
    prop_string("compatible", "sifive,plic-1.0.0");
    prop_reg2("reg", GUEST_PLIC_BASE, GUEST_PLIC_SIZE);
    prop_empty("interrupt-controller");
    prop_u32("#interrupt-cells", 1);
    prop_u32("riscv,ndev", 32);
    prop_u32("phandle", 2);
    prop_interrupts_extended(1, 9);
    end_node();

    end_node();
    end_node();
    emit_be32(FDT_END);

    u32 struct_size = g_struct - g_struct_start;
    u32 strings_off = (g_struct - g_dtb + 7) & ~7u;
    memcpy(g_dtb + strings_off, g_strings, g_strings_size);
    u32 total_size = strings_off + g_strings_size;

    put_be32(&hdr->magic, FDT_MAGIC);
    put_be32(&hdr->totalsize, total_size);
    put_be32(&hdr->off_dt_struct, struct_off);
    put_be32(&hdr->off_dt_strings, strings_off);
    put_be32(&hdr->off_mem_rsvmap, reserve_off);
    put_be32(&hdr->version, FDT_VERSION);
    put_be32(&hdr->last_comp_version, FDT_LAST_COMPAT);
    put_be32(&hdr->boot_cpuid_phys, 0);
    put_be32(&hdr->size_dt_strings, g_strings_size);
    put_be32(&hdr->size_dt_struct, struct_size);

    safe_printf("guest dtb: addr=%lx size=%u\n", RISCV_GUEST_DTB_ADDR, total_size);
}

u64 riscv_guest_dtb_addr(void) {
    return RISCV_GUEST_DTB_ADDR;
}
