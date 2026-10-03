#include <guayaba/Terminal/IOSTerminal.hpp>

#if defined(_WIN32)
    #include "WindowsTerminal.hpp"
#elif defined(__linux__) || defined(__APPLE__)
    #include "LinuxTerminal.hpp"
#else
    #error "Unsupported platform. GuayabaTUIEngine supports Windows and Linux."
#endif

namespace guayaba {

std::unique_ptr<IOSTerminal> create_system_terminal() {
#if defined(_WIN32)
    return std::make_unique<WindowsTerminal>();
#else
    return std::make_unique<LinuxTerminal>();
#endif
}

} // namespace guayaba