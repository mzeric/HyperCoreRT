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

extern "C" int modern_cpp_smoke(void)
{
    modern::Device dev;
    return dev.id();
}

extern "C" int modern_cpp_global_ctor_smoke(void)
{
    return modern::g_device.ok();
}
