#include "emul_gicv3.h"

#include "hyper_config.h"
#include "emul_dev.h"
#include <string.h>

#define VGIC_MAX_IRQS        1024U
#define VGIC_SPI_BASE        32U
#define VGIC_RDIST_SGI_BASE  0x10000UL

/* Distributor (GICD) frame offsets. */
#define GICD_CTLR       0x0000UL
#define GICD_TYPER      0x0004UL
#define GICD_IIDR       0x0008UL
#define GICD_IGROUPR    0x0080UL
#define GICD_ISENABLER  0x0100UL
#define GICD_ICENABLER  0x0180UL
#define GICD_ISPENDR    0x0200UL
#define GICD_ICPENDR    0x0280UL
#define GICD_ISACTIVER  0x0300UL
#define GICD_ICACTIVER  0x0380UL
#define GICD_IPRIORITYR 0x0400UL
#define GICD_ICFGR      0x0c00UL
#define GICD_IROUTER    0x6000UL
#define GICD_PIDR2      0xffe8UL

/* Redistributor (GICR) frame offsets (RD_base portion). */
#define GICR_CTLR       0x0000UL
#define GICR_IIDR       0x0004UL
#define GICR_TYPER      0x0008UL
#define GICR_WAKER      0x0014UL

#define GICR_TYPER_LAST    (1ULL << 4)
#define GICR_TYPER_PROCNUM (0xffffULL << 8)
#define GICR_TYPER_AFF     (0xffffffffULL << 32)

#define GIC_IIDR_VALUE      0x43b
#define GIC_PIDR2_GICV3     0x30
#define GICD_TYPER_VALUE    ((9U << 19) | 31U)

struct gic_emul_v3_state {
    /* Distributor (shared, SPI scope). */
    u32 dist_ctlr;
    u32 group[VGIC_MAX_IRQS / 32];
    u32 enable[VGIC_MAX_IRQS / 32];
    u32 pending[VGIC_MAX_IRQS / 32];
    u32 active[VGIC_MAX_IRQS / 32];
    u32 config[VGIC_MAX_IRQS / 16];
    u8  priority[VGIC_MAX_IRQS];
    u64 router[VGIC_MAX_IRQS - VGIC_SPI_BASE];

    /* Per-redistributor (per-vCPU, SGI/PPI scope). */
    u32 r_ctlr[HYPER_MAX_VCPUS];
    u32 r_waker[HYPER_MAX_VCPUS];
    u32 r_group[HYPER_MAX_VCPUS];
    u32 r_enable[HYPER_MAX_VCPUS];
    u32 r_pending[HYPER_MAX_VCPUS];
    u32 r_active[HYPER_MAX_VCPUS];
    u32 r_config[HYPER_MAX_VCPUS][2];
    u8  r_priority[HYPER_MAX_VCPUS][VGIC_SPI_BASE];
};

static struct gic_emul_v3_state vgic;


/* ---------------------------------------------------------------- *
 * Bytewise access primitives.                                      *
 * ---------------------------------------------------------------- */

static u32 access_size(int len)
{
    return 1U << len;
}

static bool in_range(u64 off, u64 base, u64 span)
{
    return off >= base && off < base + span;
}

static u64 read_bytes(const u8 *base, u32 off, int len)
{
    u64 value = 0;
    u32 size = access_size(len);

    for (u32 i = 0; i < size; ++i)
        value |= ((u64)base[off + i]) << (i * 8);
    return value;
}

static void write_bytes(u8 *base, u32 off, int len, u64 value)
{
    u32 size = access_size(len);

    for (u32 i = 0; i < size; ++i)
        base[off + i] = (value >> (i * 8)) & 0xff;
}

static u32 reg_index(u64 off, u64 base)
{
    return (off - base) / sizeof(u32);
}

static u32 frame_index(u64 addr)
{
    struct hyper_guest_vgic_config *cfg = &hyper_config()->guest.vgic;
    return (addr - cfg->gicr_base) / cfg->gicr_stride;
}

static u64 frame_offset(u64 addr)
{
    struct hyper_guest_vgic_config *cfg = &hyper_config()->guest.vgic;
    return (addr - cfg->gicr_base) % cfg->gicr_stride;
}

static u64 dist_offset(u64 addr)
{
    return addr - hyper_config()->guest.vgic.gicd_base;
}

static u64 gicr_typer(u32 frame)
{
    struct hyper_config *cfg = hyper_config();
    u64 mpidr = frame;
    u64 typer;

    if (frame < cfg->guest.vcpu_count)
        mpidr = cfg->guest.vcpu_mpidr[frame] & 0xffffffffULL;

    typer = ((frame & 0xffffULL) << 8) | (mpidr << 32);
    if (cfg->guest.vcpu_count && frame == cfg->guest.vcpu_count - 1)
        typer |= GICR_TYPER_LAST;
    return typer;
}


/* ---------------------------------------------------------------- *
 * Distributor (GICD) MMIO.                                         *
 *                                                                  *
 * Almost every GICD_* register is a bit/byte array starting at a   *
 * fixed offset.  We describe each window by a table entry and let  *
 * the dispatcher loop pick the matching one; that keeps the read   *
 * and write paths short and symmetric.                             *
 * ---------------------------------------------------------------- */

enum gicd_mode {
    GICD_RAW_BYTES,   /* read/write through write_bytes/read_bytes */
    GICD_W1S,         /* writes set bits, reads return raw words   */
    GICD_W1C,         /* writes clear bits, reads return raw words */
};

struct gicd_window {
    u64           base;
    u64           span;
    u8           *bytes;     /* backing storage (byte-addressable)  */
    enum gicd_mode mode;
};

static const struct gicd_window gicd_table[] = {
    { GICD_IGROUPR,    sizeof vgic.group,    (u8 *)vgic.group,    GICD_RAW_BYTES },
    { GICD_ISENABLER,  sizeof vgic.enable,   (u8 *)vgic.enable,   GICD_W1S       },
    { GICD_ICENABLER,  sizeof vgic.enable,   (u8 *)vgic.enable,   GICD_W1C       },
    { GICD_ISPENDR,    sizeof vgic.pending,  (u8 *)vgic.pending,  GICD_W1S       },
    { GICD_ICPENDR,    sizeof vgic.pending,  (u8 *)vgic.pending,  GICD_W1C       },
    { GICD_ISACTIVER,  sizeof vgic.active,   (u8 *)vgic.active,   GICD_W1S       },
    { GICD_ICACTIVER,  sizeof vgic.active,   (u8 *)vgic.active,   GICD_W1C       },
    { GICD_IPRIORITYR, sizeof vgic.priority, vgic.priority,       GICD_RAW_BYTES },
    { GICD_ICFGR,      sizeof vgic.config,   (u8 *)vgic.config,   GICD_RAW_BYTES },
    { GICD_IROUTER,    sizeof vgic.router,   (u8 *)vgic.router,   GICD_RAW_BYTES },
};

#define GICD_TABLE_LEN  (sizeof gicd_table / sizeof gicd_table[0])

static int dist_read(u64 off, int len, u64 *value)
{
    switch (off) {
    case GICD_CTLR:  *value = vgic.dist_ctlr;     return 0;
    case GICD_TYPER: *value = GICD_TYPER_VALUE;   return 0;
    case GICD_IIDR:  *value = GIC_IIDR_VALUE;     return 0;
    case GICD_PIDR2: *value = GIC_PIDR2_GICV3;    return 0;
    default:
        break;
    }

    for (u32 i = 0; i < GICD_TABLE_LEN; ++i) {
        const struct gicd_window *w = &gicd_table[i];
        if (!in_range(off, w->base, w->span))
            continue;
        /* Reads are byte-granular in every mode (W1S/W1C only differ
         * on the write side; on the read side they return the raw
         * state of the underlying bitmap).                          */
        *value = read_bytes(w->bytes, off - w->base, len);
        return 0;
    }

    *value = 0;
    return 0;
}

static int dist_write(u64 off, int len, u64 value)
{
    if (off == GICD_CTLR) {
        vgic.dist_ctlr = value;
        return 0;
    }

    for (u32 i = 0; i < GICD_TABLE_LEN; ++i) {
        const struct gicd_window *w = &gicd_table[i];
        if (!in_range(off, w->base, w->span))
            continue;

        switch (w->mode) {
        case GICD_RAW_BYTES:
            write_bytes(w->bytes, off - w->base, len, value);
            break;
        case GICD_W1S:
            ((u32 *)w->bytes)[reg_index(off, w->base)] |=  (u32)value;
            break;
        case GICD_W1C:
            ((u32 *)w->bytes)[reg_index(off, w->base)] &= ~(u32)value;
            break;
        }
        return 0;
    }

    /* Unknown GICD offset: silently swallow, mirroring real GIC. */
    return 0;
}


/* ---------------------------------------------------------------- *
 * Redistributor SGI/PPI frame MMIO (per-vCPU).                     *
 *                                                                  *
 * The redistributor's SGI_base mirrors GICD's lowest 32 IRQs, so   *
 * we reuse the GICD_* offsets but back them by the per-frame state *
 * arrays.  Same table-driven dispatch as above.                    *
 * ---------------------------------------------------------------- */

enum gicr_mode {
    GICR_RAW_BYTES_PRIO,   /* byte-addressable priority array        */
    GICR_RAW_BYTES_CONFIG, /* byte-addressable per-frame config word */
    GICR_W1S_WORD,         /* writes set bits in a single u32 field  */
    GICR_W1C_WORD,         /* writes clear bits in a single u32 field*/
    GICR_RAW_WORD,         /* writes overwrite the whole u32 field   */
};

struct gicr_window {
    u64            base;
    u64            span;        /* size in bytes inside the SGI frame   */
    size_t         field_off;   /* byte offset of the per-frame field   */
    enum gicr_mode mode;
};

#define R_FIELD_OFF(_member) offsetof(struct gic_emul_v3_state, _member)

static const struct gicr_window gicr_table[] = {
    { GICD_IGROUPR,    sizeof(u32),                        R_FIELD_OFF(r_group),    GICR_RAW_WORD          },
    { GICD_ISENABLER,  sizeof(u32),                        R_FIELD_OFF(r_enable),   GICR_W1S_WORD          },
    { GICD_ICENABLER,  sizeof(u32),                        R_FIELD_OFF(r_enable),   GICR_W1C_WORD          },
    { GICD_ISPENDR,    sizeof(u32),                        R_FIELD_OFF(r_pending),  GICR_W1S_WORD          },
    { GICD_ICPENDR,    sizeof(u32),                        R_FIELD_OFF(r_pending),  GICR_W1C_WORD          },
    { GICD_ISACTIVER,  sizeof(u32),                        R_FIELD_OFF(r_active),   GICR_W1S_WORD          },
    { GICD_ICACTIVER,  sizeof(u32),                        R_FIELD_OFF(r_active),   GICR_W1C_WORD          },
    { GICD_IPRIORITYR, VGIC_SPI_BASE,                      R_FIELD_OFF(r_priority), GICR_RAW_BYTES_PRIO    },
    { GICD_ICFGR,      sizeof vgic.r_config[0],            R_FIELD_OFF(r_config),   GICR_RAW_BYTES_CONFIG  },
};

#define GICR_TABLE_LEN (sizeof gicr_table / sizeof gicr_table[0])

static u32 *redist_word_field(u32 frame, size_t field_off)
{
    /* Per-frame u32 (r_group / r_enable / r_pending / r_active). */
    return (u32 *)((u8 *)&vgic + field_off) + frame;
}

static u8 *redist_byte_array(u32 frame, size_t field_off, size_t row_stride)
{
    /* Per-frame byte array (r_priority[frame][...], r_config[frame][...]). */
    return (u8 *)&vgic + field_off + frame * row_stride;
}

static int redist_sgi_read(u32 frame, u64 off, int len, u64 *value)
{
    if (frame >= HYPER_MAX_VCPUS) {
        *value = 0;
        return 0;
    }

    for (u32 i = 0; i < GICR_TABLE_LEN; ++i) {
        const struct gicr_window *w = &gicr_table[i];
        if (!in_range(off, w->base, w->span))
            continue;

        switch (w->mode) {
        case GICR_RAW_BYTES_PRIO:
            *value = read_bytes(redist_byte_array(frame, w->field_off,
                                                  VGIC_SPI_BASE),
                                off - w->base, len);
            break;
        case GICR_RAW_BYTES_CONFIG:
            *value = read_bytes(redist_byte_array(frame, w->field_off,
                                                  sizeof vgic.r_config[0]),
                                off - w->base, len);
            break;
        case GICR_RAW_WORD:
        case GICR_W1S_WORD:
        case GICR_W1C_WORD:
            *value = *redist_word_field(frame, w->field_off);
            break;
        }
        return 0;
    }

    *value = 0;
    return 0;
}

static int redist_sgi_write(u32 frame, u64 off, int len, u64 value)
{
    if (frame >= HYPER_MAX_VCPUS)
        return 0;

    for (u32 i = 0; i < GICR_TABLE_LEN; ++i) {
        const struct gicr_window *w = &gicr_table[i];
        if (!in_range(off, w->base, w->span))
            continue;

        switch (w->mode) {
        case GICR_RAW_WORD:
            *redist_word_field(frame, w->field_off) = (u32)value;
            break;
        case GICR_W1S_WORD:
            *redist_word_field(frame, w->field_off) |=  (u32)value;
            break;
        case GICR_W1C_WORD:
            *redist_word_field(frame, w->field_off) &= ~(u32)value;
            break;
        case GICR_RAW_BYTES_PRIO:
            write_bytes(redist_byte_array(frame, w->field_off,
                                          VGIC_SPI_BASE),
                        off - w->base, len, value);
            break;
        case GICR_RAW_BYTES_CONFIG:
            write_bytes(redist_byte_array(frame, w->field_off,
                                          sizeof vgic.r_config[0]),
                        off - w->base, len, value);
            break;
        }
        return 0;
    }

    return 0;
}


/* ---------------------------------------------------------------- *
 * Redistributor RD_base (per-vCPU control / identification).       *
 * ---------------------------------------------------------------- */

static int redist_read(u64 addr, int len, u64 *value)
{
    u32 frame = frame_index(addr);
    u64 off   = frame_offset(addr);

    if (frame >= HYPER_MAX_VCPUS) {
        *value = 0;
        return 0;
    }
    if (off >= VGIC_RDIST_SGI_BASE)
        return redist_sgi_read(frame, off - VGIC_RDIST_SGI_BASE, len, value);

    switch (off) {
    case GICR_CTLR:  *value = vgic.r_ctlr[frame];                 return 0;
    case GICR_IIDR:  *value = GIC_IIDR_VALUE;                     return 0;
    case GICR_TYPER: *value = gicr_typer(frame);                  return 0;
    case GICR_WAKER: *value = vgic.r_waker[frame] & ~(1U << 2);   return 0;
    case GICD_PIDR2: *value = GIC_PIDR2_GICV3;                    return 0;
    default:
        *value = 0;
        return 0;
    }
}

static int redist_write(u64 addr, int len, u64 value)
{
    u32 frame = frame_index(addr);
    u64 off   = frame_offset(addr);

    if (frame >= HYPER_MAX_VCPUS)
        return 0;
    if (off >= VGIC_RDIST_SGI_BASE)
        return redist_sgi_write(frame, off - VGIC_RDIST_SGI_BASE, len, value);

    switch (off) {
    case GICR_CTLR:  vgic.r_ctlr[frame]  = value;          return 0;
    case GICR_WAKER: vgic.r_waker[frame] = value & (1U << 1); return 0;
    default:
        return 0;
    }
}


/* ---------------------------------------------------------------- *
 * Emul-driver glue.                                                *
 * ---------------------------------------------------------------- */

static int gic3_read(struct emul_device *dev, uint64_t addr, int len, uint64_t *value)
{
    struct hyper_guest_vgic_config *cfg = &hyper_config()->guest.vgic;

    if (addr >= cfg->gicr_base && addr < cfg->gicr_base + cfg->gicr_size)
        return redist_read(addr, len, value);
    return dist_read(dist_offset(addr), len, value);
}

static int gic3_write(struct emul_device *dev, uint64_t addr, int len, uint64_t value)
{
    struct hyper_guest_vgic_config *cfg = &hyper_config()->guest.vgic;

    if (addr >= cfg->gicr_base && addr < cfg->gicr_base + cfg->gicr_size)
        return redist_write(addr, len, value);
    return dist_write(dist_offset(addr), len, value);
}

static struct emul_driver_ops gic3_ops = {
    .read  = gic3_read,
    .write = gic3_write,
};

static struct emul_driver gic3_driver = {
    .name = "gic3",
    .ops  = &gic3_ops,
};

void gic_register_emul(void)
{
    memset(&vgic, 0, sizeof(vgic));
    for (u32 i = 0; i < VGIC_MAX_IRQS; ++i)
        vgic.priority[i] = 0xa0;
    for (u32 vcpu = 0; vcpu < HYPER_MAX_VCPUS; ++vcpu)
        for (u32 i = 0; i < VGIC_SPI_BASE; ++i)
            vgic.r_priority[vcpu][i] = 0xa0;
    register_emul_driver(&gic3_driver);
}
