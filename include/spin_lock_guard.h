#pragma once

#include "spin_lock.h"

/* RAII spinlock guard for C++ code.
 * Usage:
 *   {
 *       SpinLockGuard guard(some_lock);
 *       // critical section
 *   } // auto unlock
 */
class SpinLockGuard {
public:
    explicit SpinLockGuard(spinlock_t &lock) : lock_(lock)
    {
        arch_spin_lock(&lock_);
    }

    ~SpinLockGuard()
    {
        arch_spin_unlock(&lock_);
    }

    SpinLockGuard(const SpinLockGuard &) = delete;
    SpinLockGuard &operator=(const SpinLockGuard &) = delete;

private:
    spinlock_t &lock_;
};
