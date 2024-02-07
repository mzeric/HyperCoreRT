#include "vmmio.h"

struct emulate_ops {
    int (*read)(void);
};




int vmm_devemu_emulate_read() {
    int ret = 0;

    vmm_debug("this\n");
    return ret;
}