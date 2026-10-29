#ifndef GUAYABA_CORE_TERMCHAR_HPP
#define GUAYABA_CORE_TERMCHAR_HPP

#include <cstdint>

namespace guayaba {

// One grid cell: a character plus foreground and background color ids.
struct TermChar {
    wchar_t character = L' ';
    uint16_t foreground = 7;   // White (default)
    uint16_t background = 0;   // Black (default)
};

} // namespace guayaba

#endif // GUAYABA_CORE_TERMCHAR_HPP