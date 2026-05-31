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
#define FDT_NOP          0x4u
#define FDT_END          0x9u
#define GUEST_DTB_SIZE   0x10000u
#define GUEST_UART_BASE  0x20000000UL
#define GUEST_UART_SIZE  0x1000UL
#define GUEST_PLIC_BASE  0x0c000000UL
#define GUEST_PLIC_SIZE  0x400000UL
#define GUEST_PLIC_PHANDLE 0x100u

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

static u64 g_guest_entry = RISCV_DEFAULT_GUEST_ENTRY;
static u64 g_guest_dtb_addr = RISCV_DEFAULT_GUEST_DTB_ADDR;
static u64 g_guest_ram_base = RISCV_DEFAULT_GUEST_RAM_BASE;
static u64 g_guest_ram_size = RISCV_DEFAULT_GUEST_RAM_SIZE;
static u64 g_guest_initrd_start;
static u64 g_guest_initrd_end;
static u32 g_guest_vcpus = 1;

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

static u32 fdt32_to_cpu(const void *p) {
    return bswap32(*(const u32 *)p);
}

static void put_be32(void *p, u32 v) {
    *(u32 *)p = bswap32(v);
}

static void put_be64(void *p, u64 v) {
    *(u64 *)p = bswap64(v);
}

static const char *align4(const char *p) {
    return (const char *)(((uintptr_t)p + 3) & ~3UL);
}

static bool parse_num(const char *s, u64 *value) {
    u64 v = 0;
    int base = 10;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }

    if (!*s)
        return false;

    while (*s && *s != ' ') {
        int digit;
        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (base == 16 && *s >= 'a' && *s <= 'f')
            digit = *s - 'a' + 10;
        else if (base == 16 && *s >= 'A' && *s <= 'F')
            digit = *s - 'A' + 10;
        else
            return false;
        if (digit >= base)
            return false;
        v = v * base + digit;
        s++;
    }

    *value = v;
    return true;
}

static bool parse_bootarg_u64(const char *bootargs, const char *key, u64 *value) {
    size_t key_len = strlen(key);
    const char *p = bootargs;

    while (p && *p) {
        while (*p == ' ')
            p++;
        if (!strncmp(p, key, key_len) && p[key_len] == '=')
            return parse_num(p + key_len + 1, value);
        p = strchr(p, ' ');
    }
    return false;
}

static const char *find_bootargs(void *fdt) {
    if (!fdt || fdt32_to_cpu(fdt) != FDT_MAGIC)
        return NULL;

    const fdt_header_min *hdr = (const fdt_header_min *)fdt;
    const char *base = (const char *)fdt;
    const char *structp = base + fdt32_to_cpu(&hdr->off_dt_struct);
    const char *struct_end = structp + fdt32_to_cpu(&hdr->size_dt_struct);
    const char *strings = base + fdt32_to_cpu(&hdr->off_dt_strings);
    u32 strings_size = fdt32_to_cpu(&hdr->size_dt_strings);

    while (structp + sizeof(u32) <= struct_end) {
        u32 token = fdt32_to_cpu(structp);
        structp += sizeof(u32);

        if (token == FDT_BEGIN_NODE) {
            structp = align4(structp + strnlen(structp, struct_end - structp) + 1);
        } else if (token == FDT_PROP) {
            if (structp + 2 * sizeof(u32) > struct_end)
                break;
            u32 len = fdt32_to_cpu(structp);
            structp += sizeof(u32);
            u32 nameoff = fdt32_to_cpu(structp);
            structp += sizeof(u32);
            const char *value = structp;
            structp = align4(structp + len);

            if (nameoff >= strings_size)
                continue;
            const char *name = strings + nameoff;
            if (!strcmp(name, "bootargs") && len > 0)
                return value;
        } else if (token == FDT_END) {
            break;
        } else if (token == FDT_END_NODE || token == FDT_NOP) {
            continue;
        } else {
            break;
        }
    }
    return NULL;
}

void riscv_guest_config_init(void *host_fdt) {
    const char *bootargs = find_bootargs(host_fdt);
    u64 tmp;

    g_guest_entry = RISCV_DEFAULT_GUEST_ENTRY;
    g_guest_dtb_addr = RISCV_DEFAULT_GUEST_DTB_ADDR;
    g_guest_ram_base = RISCV_DEFAULT_GUEST_RAM_BASE;
    g_guest_ram_size = RISCV_DEFAULT_GUEST_RAM_SIZE;
    g_guest_initrd_start = 0;
    g_guest_initrd_end = 0;
    g_guest_vcpus = 1;

    if (bootargs) {
        if (parse_bootarg_u64(bootargs, "guest_entry", &tmp))
            g_guest_entry = tmp;
        if (parse_bootarg_u64(bootargs, "guest_dtb", &tmp))
            g_guest_dtb_addr = tmp;
        if (parse_bootarg_u64(bootargs, "guest_ram_base", &tmp))
            g_guest_ram_base = tmp;
        if (parse_bootarg_u64(bootargs, "guest_ram_size", &tmp))
            g_guest_ram_size = tmp;
        if (parse_bootarg_u64(bootargs, "guest_initrd_start", &tmp))
            g_guest_initrd_start = tmp;
        if (parse_bootarg_u64(bootargs, "guest_initrd_end", &tmp))
            g_guest_initrd_end = tmp;
        if (g_guest_initrd_end <= g_guest_initrd_start) {
            g_guest_initrd_start = 0;
            g_guest_initrd_end = 0;
        }
        if (parse_bootarg_u64(bootargs, "guest_vcpus", &tmp)) {
            if (tmp < 1)
                tmp = 1;
            if (tmp > CONFIG_SMP_CPU_NUM)
                tmp = CONFIG_SMP_CPU_NUM;
            g_guest_vcpus = (u32)tmp;
        }
    }

    safe_printf("guest config: entry=%lx dtb=%lx ram=%lx+%lx initrd=%lx-%lx vcpus=%u\n",
                g_guest_entry, g_guest_dtb_addr,
                g_guest_ram_base, g_guest_ram_size,
                g_guest_initrd_start, g_guest_initrd_end, g_guest_vcpus);
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

static void prop_u64(const char *name, u64 value) {
    u64 be;
    put_be64(&be, value);
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

static void prop_interrupts_extended(void) {
    u32 data[CONFIG_SMP_CPU_NUM * 2];
    for (u32 i = 0; i < g_guest_vcpus; i++) {
        put_be32(&data[i * 2], i + 1);
        put_be32(&data[i * 2 + 1], 9);
    }
    prop_raw("interrupts-extended", data, g_guest_vcpus * 2 * sizeof(u32));
}

void riscv_guest_dtb_init(void *host_fdt) {
    (void)host_fdt;

    g_dtb = (u8 *)phy_to_vir(g_guest_dtb_addr);
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
    prop_u32("interrupt-parent", GUEST_PLIC_PHANDLE);

    begin_node("chosen");
    if (g_guest_initrd_end > g_guest_initrd_start)
        prop_string("bootargs", "console=ttyS0,115200 earlycon=uart8250,mmio,0x20000000 loglevel=8 rdinit=/init");
    else
        prop_string("bootargs", "console=ttyS0,115200 earlycon=uart8250,mmio,0x20000000 loglevel=8");
    prop_string("stdout-path", "/soc/serial@20000000:115200");
    if (g_guest_initrd_end > g_guest_initrd_start) {
        prop_u64("linux,initrd-start", g_guest_initrd_start);
        prop_u64("linux,initrd-end", g_guest_initrd_end);
    }
    end_node();

    begin_node("memory@90000000");
    prop_string("device_type", "memory");
    prop_reg2("reg", g_guest_ram_base, g_guest_ram_size);
    end_node();

    begin_node("cpus");
    prop_u32("#address-cells", 1);
    prop_u32("#size-cells", 0);
    prop_u32("timebase-frequency", riscv_timebase_frequency());

    for (u32 i = 0; i < g_guest_vcpus; i++) {
        char cpu_name[] = "cpu@0";
        cpu_name[4] = '0' + i;
        begin_node(cpu_name);
        prop_string("device_type", "cpu");
        prop_string("compatible", "riscv");
        prop_string("status", "okay");
        prop_string("riscv,isa", "rv64imafdch");
        prop_string("mmu-type", "riscv,sv48");
        prop_u32("reg", i);
        begin_node("interrupt-controller");
        prop_empty("interrupt-controller");
        prop_string("compatible", "riscv,cpu-intc");
        prop_u32("#interrupt-cells", 1);
        prop_u32("phandle", i + 1);
        end_node();
        end_node();
    }
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
    prop_u32("interrupt-parent", GUEST_PLIC_PHANDLE);
    prop_u32("interrupts", 10);
    end_node();

    begin_node("plic@c000000");
    prop_string("compatible", "sifive,plic-1.0.0");
    prop_reg2("reg", GUEST_PLIC_BASE, GUEST_PLIC_SIZE);
    prop_empty("interrupt-controller");
    prop_u32("#interrupt-cells", 1);
    prop_u32("riscv,ndev", 32);
    prop_u32("phandle", GUEST_PLIC_PHANDLE);
    prop_interrupts_extended();
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

    safe_printf("guest dtb: addr=%lx size=%u\n", g_guest_dtb_addr, total_size);
}

u64 riscv_guest_entry(void) {
    return g_guest_entry;
}

u64 riscv_guest_dtb_addr(void) {
    return g_guest_dtb_addr;
}

u64 riscv_guest_ram_base(void) {
    return g_guest_ram_base;
}

u64 riscv_guest_ram_size(void) {
    return g_guest_ram_size;
}

u32 riscv_guest_vcpu_count(void) {
    return g_guest_vcpus;
}
