#include "riscv_features.h"
#include "safe_printf.h"
#include <string.h>

#define FDT_MAGIC      0xd00dfeedu
#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_NOP        0x4u
#define FDT_END        0x9u

typedef struct fdt_header_min {
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
} fdt_header_min;

static bool g_has_svvptc;
static bool g_has_fpu;
static bool g_has_vector;
static u32 g_timebase_frequency = 10000000;

static u32 be32(const void *p) {
    const u8 *b = (const u8 *)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

static const char *align4(const char *p) {
    return (const char *)(((uintptr_t)p + 3) & ~3UL);
}

static bool stringlist_contains(const char *list, int len, const char *needle) {
    int needle_len = strlen(needle);
    int off = 0;

    while (off < len) {
        const char *s = list + off;
        int left = len - off;
        int slen = strnlen(s, left);
        if (slen == needle_len && !memcmp(s, needle, needle_len))
            return true;
        off += slen + 1;
    }
    return false;
}

static bool isa_single_letter_has(const char *isa, char ext) {
    if (!isa)
        return false;

    const char *p = isa;
    if (!strncmp(p, "rv32", 4) || !strncmp(p, "rv64", 4))
        p += 4;
    while (*p && *p != '_') {
        if (*p == ext)
            return true;
        p++;
    }
    return false;
}

static bool isa_has_word(const char *isa, const char *word) {
    if (!isa || !word)
        return false;

    size_t word_len = strlen(word);
    const char *p = isa;
    while (*p) {
        if ((p == isa || p[-1] == '_') && !strncmp(p, word, word_len) &&
            (p[word_len] == '\0' || p[word_len] == '_'))
            return true;
        p++;
    }
    return false;
}

static void parse_isa_string(const char *isa) {
    if (!isa)
        return;

    if (isa_has_word(isa, "svvptc"))
        g_has_svvptc = true;
    if (isa_single_letter_has(isa, 'f') || isa_single_letter_has(isa, 'd'))
        g_has_fpu = true;
    if (isa_single_letter_has(isa, 'v'))
        g_has_vector = true;
}

static void parse_isa_extensions(const char *exts, int len) {
    if (!exts || len <= 0)
        return;

    if (stringlist_contains(exts, len, "svvptc"))
        g_has_svvptc = true;
    if (stringlist_contains(exts, len, "f") || stringlist_contains(exts, len, "d"))
        g_has_fpu = true;
    if (stringlist_contains(exts, len, "v") || stringlist_contains(exts, len, "zve64d") ||
        stringlist_contains(exts, len, "zve64f") || stringlist_contains(exts, len, "zve64x"))
        g_has_vector = true;
}

void riscv_features_init(void *fdt) {
    if (!fdt || be32(fdt) != FDT_MAGIC) {
        safe_printf("riscv features: no valid host dtb\n");
        return;
    }

    const fdt_header_min *hdr = (const fdt_header_min *)fdt;
    const char *base = (const char *)fdt;
    const char *structp = base + be32(&hdr->off_dt_struct);
    const char *struct_end = structp + be32(&hdr->size_dt_struct);
    const char *strings = base + be32(&hdr->off_dt_strings);
    u32 strings_size = be32(&hdr->size_dt_strings);

    while (structp + sizeof(u32) <= struct_end) {
        u32 token = be32(structp);
        structp += sizeof(u32);

        if (token == FDT_BEGIN_NODE) {
            structp = align4(structp + strnlen(structp, struct_end - structp) + 1);
        } else if (token == FDT_PROP) {
            if (structp + 2 * sizeof(u32) > struct_end)
                break;
            u32 len = be32(structp);
            structp += sizeof(u32);
            u32 nameoff = be32(structp);
            structp += sizeof(u32);
            const char *value = structp;
            structp = align4(structp + len);

            if (nameoff >= strings_size)
                continue;
            const char *name = strings + nameoff;

            if (!strcmp(name, "timebase-frequency") && len >= sizeof(u32))
                g_timebase_frequency = be32(value);
            else if (!strcmp(name, "riscv,isa"))
                parse_isa_string(value);
            else if (!strcmp(name, "riscv,isa-extensions"))
                parse_isa_extensions(value, len);
        } else if (token == FDT_END) {
            break;
        } else if (token == FDT_END_NODE || token == FDT_NOP) {
            continue;
        } else {
            break;
        }
    }

    safe_printf("riscv features: svvptc=%d fpu=%d vector=%d timebase=%u\n",
                g_has_svvptc, g_has_fpu, g_has_vector, g_timebase_frequency);
}

bool riscv_has_svvptc(void) {
    return g_has_svvptc;
}

bool riscv_has_fpu(void) {
    return g_has_fpu;
}

bool riscv_has_vector(void) {
    return g_has_vector;
}

u32 riscv_timebase_frequency(void) {
    return g_timebase_frequency;
}
