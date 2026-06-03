#pragma once
#include "htypes.h"
#include "list.h"

struct mem_region;
struct emul_device;

#define EMUL_DEV_MAX_MATCH_NAME (32u)

struct emul_driver_ops {
    int (*read)(struct emul_device *dev, uint64_t addr, int len, uint64_t *value);
    int (*write)(struct emul_device *dev, uint64_t addr, int len, uint64_t value);
    int (*reset)(struct emul_device *dev, void *priv);
};

struct emul_driver {
    struct list_head   list;
    const char         *name;
    struct emul_driver_ops *ops;
};

struct emul_device {
    struct emul_driver *driver;
    void          *priv;
};
void init_emul_dev();
void register_emul_driver(struct emul_driver *driver);
void probe_emul_dev(struct mem_region *region);
