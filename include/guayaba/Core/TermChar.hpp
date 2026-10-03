#ifndef GUAYABA_CORE_TERMCHAR_HPP
#define GUAYABA_CORE_TERMCHAR_HPP

#include <cstdint>

namespace guayaba {

// Replaces Windows CHAR_INFO
struct TermChar {
    wchar_t character = L' ';
    uint16_t foreground = 7;   // White (default)
    uint16_t background = 0;   // Black (default)
};

} // namespace guayaba

#endif // GUAYABA_CORE_TERMCHAR_HPP