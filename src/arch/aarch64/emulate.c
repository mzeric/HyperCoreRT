#include "emulate.h"
#include <guest_memory.h>
#include <emul_dev.h>
#include <sched.h>

#include "emul_gic.h"

#define ARM_REG_X(regs, id) ((uint64_t *)&regs->x0)[id]

uint64_t vcpu_reg_read(struct cpu_user_regs *regs, int id, int size) {

    if (id >= 0 && id <= 30)
        return ARM_REG_X(regs, id);
    return 0;
}

uint64_t vcpu_reg_write(struct cpu_user_regs *regs, int id, int size, uint64_t value) {

    if (id >= 0 && id <= 30)
        ARM_REG_X(regs, id) = value;
    return 0;
}

int vcpu_emulate_read(vcpu_t *vcpu, struct cpu_user_regs *regs, paddr_t ipa, int reg_id, int size) {

    struct mem_region *mem = guest_mem_find_region(vcpu, ipa, 0);
    // safe_printf("emulate read %s\n", mem->match_name);

    uint64_t value;
    int      ret;

    if (!mem->dev->driver->ops->read)
        hyper_fatal("emulator:%s read func NULL\n", mem->match_name);

    ret = mem->dev->driver->ops->read(mem->dev, ipa, size, &value);

    if (!ret)
        vcpu_reg_write(regs, reg_id, size, value);

    return ret;
}

int vcpu_emulate_write(vcpu_t *vcpu, struct cpu_user_regs *regs, paddr_t ipa, int reg_id,
                       int size) {
    uint64_t           value = 9;
    int                ret;
    struct mem_region *mem = guest_mem_find_region(vcpu, ipa, 0);
    value = vcpu_reg_read(regs, reg_id, size);
    // safe_printf("emulate write  %s: [%lx] = %x, size:%d\n", mem->match_name, ipa, value, size);


    if (!mem->dev->driver->ops->write)
        hyper_fatal("emulator:%s write func NULL\n", mem->match_name);

    ret = mem->dev->driver->ops->write(mem->dev, ipa, size, value);

    return ret;
}
#define ISS_SYSREG_MASK					0xfffffc1e

int vcpu_emulate_sysreg_read(struct cpu_user_regs *regs, uint64_t iss, uint64_t *data) {
    iss &= ISS_SYSREG_MASK;
    (void)current_task()->vcpu;
    int ret = 0;
    safe_printf("mrs %lx, try %llx\n", iss & ISS_SYSREG_MASK, ISS_ACTLR_EL1);

    switch(iss) {
        case ISS_ACTLR_EL1 :
        *data = 0;
        break;
        case ISS_SRE_EL1:
        *data = 0;
        safe_printf("mrs %lx\n", iss);
        break;
        default:{
            safe_printf("unsupport mrs %lx\n", iss);
            ret = -1;
        }
    }

    return ret;
}

int vcpu_emulate_sysreg_write(struct cpu_user_regs *regs, uint64_t iss, uint64_t data) {
    iss &= ISS_SYSREG_MASK;
    int ret = 0;

    switch(iss){
    case ISS_SYSREG_ENC(3, 5, 0, 12, 11): {
        struct gic_vcpu_sgi sgi;

        sgi.intid = (data >> 24) & 0xf;
        sgi.aff1 = (data >> 16) & 0xff;
        sgi.aff2 = (data >> 32) & 0xff;
        sgi.aff3 = (data >> 48) & 0xff;
        sgi.rs = (data >> 44) & 0xf;
        sgi.target_list = data & 0xffff;
        sgi.irm = (data >> 40) & 0x1;
        gic_vcpu_send_sgi(&sgi);
        break;
    }

    default:{
        safe_printf("unsupport msr %lx\n", iss);
        ret = -1;
    }
    }


    return ret;
}