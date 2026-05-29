#include "vmio.h"

struct emulate_ops {
    int (*read)(void);
};

int emul_dev_aux_read(void) {
    int ret = 0;

    hyper_debug("this");
    return ret;
}
