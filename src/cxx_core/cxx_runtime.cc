/*
 * Minimal C++ runtime for HyperCoreRT baremetal hypervisor
 *
 * Supports:
 *   - operator new/delete (via kmalloc/kfree)
 *   - placement new
 *   - __cxa_pure_virtual (trap on pure virtual call)
 *   - __dso_handle (no-op)
 *   - __cxa_atexit (no-op)
 *
 * Does NOT support:
 *   - exceptions
 *   - RTTI / dynamic_cast / typeid
 *   - thread-safe static initialization guards
 *   - full destructor teardown
 */

#include <stddef.h>
#include <inttypes.h>

extern "C" {
void *kmalloc(uint64_t size);
void *kfree(void *ptr);
}

/* ---- placement new ---- */

void *operator new(size_t, void *p) noexcept
{
    return p;
}

void operator delete(void *, void *) noexcept
{
}

/* ---- global new/delete ---- */

void *operator new(size_t size)
{
    void *p = kmalloc(size);
    if (!p) {
        /* OOM in baremetal hypervisor - hang */
        while (1) {}
    }
    return p;
}

void operator delete(void *p) noexcept
{
    if (p)
        kfree(p);
}

void *operator new[](size_t size)
{
    return operator new(size);
}

void operator delete[](void *p) noexcept
{
    operator delete(p);
}

/* sized delete (C++14) */
void operator delete(void *p, size_t) noexcept
{
    operator delete(p);
}

void operator delete[](void *p, size_t) noexcept
{
    operator delete(p);
}

/* ---- C++ ABI symbols ---- */

extern "C" {

/* Pure virtual function call trap */
void __cxa_pure_virtual()
{
    while (1) {}
}

/* DSO handle - used by __cxa_atexit but we no-op it */
void *__dso_handle = nullptr;

/* atexit - no-op, baremetal hypervisor doesn't exit */
int __cxa_atexit(void (*)(void *), void *, void *)
{
    return 0;
}

/* Guard functions for function-local static variables
 * We disable thread-safe-statics via compiler flag, but
 * provide these in case the compiler still emits them. */
int __cxa_guard_acquire(unsigned long long *guard)
{
    /* If guard byte[0] is 0, initialization not yet done */
    return !(*guard);
}

void __cxa_guard_release(unsigned long long *guard)
{
    *guard = 1;
}

void __cxa_guard_abort(unsigned long long *guard)
{
    /* Nothing to do */
}

/* Global constructor/destructor runner */
typedef void (*cxx_init_func_t)();

extern cxx_init_func_t __init_array_start[];
extern cxx_init_func_t __init_array_end[];
extern cxx_init_func_t __fini_array_start[];
extern cxx_init_func_t __fini_array_end[];

void cxx_run_global_ctors()
{
    for (cxx_init_func_t *fn = __init_array_start; fn < __init_array_end; ++fn)
        (*fn)();
}

void cxx_run_global_dtors()
{
    for (cxx_init_func_t *fn = __fini_array_start; fn < __fini_array_end; ++fn)
        (*fn)();
}

} /* extern "C" */
