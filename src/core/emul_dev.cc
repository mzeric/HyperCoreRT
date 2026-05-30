#include "list.h"
#include "vcpu.h"
#include "guest_memory.h"
#include "emul_dev.h"

#include "kmalloc.h"
#include "emul_gicv3.h"
#include "emul_uart.h"
#include <string.h>

static struct list_head g_all_drivers;

int pl011_read(struct emul_device *dev, uint64_t addr, int len, uint64_t *value) {
    safe_printf("pl011 read: %lx %d bytes\n", addr, len);

    *value = 'S';
    return 0;
}

int pl011_write(struct emul_device *dev, uint64_t addr, int len, uint64_t value) {
    safe_printf("pl011 write %lx = %lx %d bytes\n", addr, value, len);

    return 0;
}

struct emul_driver_ops pl011_ops = {
    .read = pl011_read,
    .write = pl011_write,
};

struct emul_driver pl011_driver = {
    .name = "pl011",
    // .ops = &pl011_ops,
    .ops = NULL,
};

void init_emul_dev() {
    INIT_LIST_HEAD(&g_all_drivers);

    gic_register_emul();
    uart_register_emul();
}

void register_emul_driver(struct emul_driver *driver) {
    list_add_tail(&driver->list, &g_all_drivers);
}

void probe_emul_dev(struct mem_region *region) {

    struct emul_driver *pos;

    list_for_each_entry(pos, &g_all_drivers, list) {
        if(strcmp(pos->name, region->match_name) == 0) {
            struct emul_device *dev = (struct emul_device *)kmalloc(sizeof(struct emul_device));
            memset(dev, 0, sizeof(struct emul_device));


            dev->driver = pos;
            region->dev = dev;
        }
    }
}

