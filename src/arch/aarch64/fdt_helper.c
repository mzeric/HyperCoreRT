#include "vmio.h"
#include "fdt_helper.h"

int fdt_node_offset(const void *fdt, int startoffset,
				  const char *propname,
				  const void *propval, int proplen)
{
	int offset;
	const void *val;
	int len;
    int depth;

	FDT_RO_PROBE(fdt);

	/* FIXME: The algorithm here is pretty horrible: we scan each
	 * property of a node in fdt_getprop(), then if that didn't
	 * find what we want, we scan over them again making our way
	 * to the next node.  Still it's the easiest to implement
	 * approach; performance can come later. */
	for (offset = fdt_next_node(fdt, startoffset, &depth);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, &depth)) {
		val = fdt_getprop(fdt, offset, propname, &len);

        hyper_info("fdt try:[%s]:[%p], len=%d(try %d)", propname, val, len, proplen);
		if (val && (len == proplen)
		    && (memcmp(val, propval, len) == 0))
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}

const void* get_tree_prop(const void *fdt, int node, int top_stop_node, const char *prop) {
    const fdt32_t *prop_val = NULL;
    int len = -1;

    if(node == -1)
        return NULL;

    // 尝试获取当前节点的属性
    prop_val = fdt_getprop(fdt, node, prop, &len);
    if (prop_val)
        return prop_val;

    if(node == top_stop_node)
        return NULL;

    // 向上遍历父节点
    return get_tree_prop(fdt, fdt_parent_offset(fdt, node), top_stop_node, prop);
}

void fdt_get_cur_as(void *fdt, int node, int *na, int *ns) {
    const u32 *num_addr, *num_size;
    int a = 2, s = 2;

    num_addr = get_tree_prop(fdt, node, -1, "#address-cells");
    num_size = get_tree_prop(fdt, node, -1, "#size-cells");

    if (num_addr)
        a = fdt32_to_cpu(*(fdt32_t*)num_addr);

    if (num_size)
        s = fdt32_to_cpu(*(fdt32_t*)num_size);

    *na = a;
    *ns = s;
}

int fdt_get_reg_info(void *fdt, int node, uint64_t *addr, uint64_t *size) {

    int na, ns;
    int len;

    fdt_get_cur_as(fdt, node, &na, &ns);

    const u32 *p = fdt_getprop(fdt, node, "reg", &len);

    if(!p) {
        hyper_err("reg <> not found");
        return -1;
    }
    if (p && len < (na + ns) * sizeof(uint32_t)) {
        hyper_err("reg <> not enough");
        return -2;
    }

    if(na == 2)
        *addr = fdt64_ld((u64*)p);
    else
        *addr = fdt32_ld(p);

    if(ns == 2)
        *size = fdt64_ld((u64*)((u32*)p + na));
    else
        *size = fdt32_ld(p + na);

    // hyper_info("reg cells(%d,%d) value: <%lx, %lx>", na, ns, *addr, *size);
    return 0;
}
