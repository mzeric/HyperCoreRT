#include "spin_lock_guard.h"

namespace modern {

class Device {
public:
    constexpr Device() = default;

    int id() const
    {
        return 42;
    }
};

class GlobalDevice {
public:
    GlobalDevice()
        : magic_(0x43505858)  /* "CPXX" */
    {
    }

    int ok() const
    {
        return magic_ == 0x43505858;
    }

private:
    unsigned int magic_;
};

static GlobalDevice g_device;

}  // namespace modern

int modern_cpp_smoke(void)
{
    modern::Device dev;
    return dev.id();
}

int modern_cpp_global_ctor_smoke(void)
{
    return modern::g_device.ok();
}

int modern_cpp_raii_lock_smoke(void)
{
    static spinlock_t test_lock = { .lock = 0 };
    int val = 0;
    {
        SpinLockGuard guard(test_lock);
        val = 99;
    }
    /* lock should be unlocked here — test passes if we didn't deadlock */
    return val;
}
