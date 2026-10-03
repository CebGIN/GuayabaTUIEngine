#ifndef GUAYABA_CORE_COLOR_HPP
#define GUAYABA_CORE_COLOR_HPP

#include <cstdint>
#include <string>

namespace guayaba::Color {

// Color IDs map to standard 4-bit terminal colors:
//   0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
//   8-15 = Bright variants of the above

constexpr const char* RED = "RED";
constexpr const char* GREEN = "GREEN";
constexpr const char* BLUE = "BLUE";
constexpr const char* YELLOW = "YELLOW";
constexpr const char* CYAN = "CYAN";
constexpr const char* MAGENTA = "MAGENTA";
constexpr const char* WHITE = "WHITE";
constexpr const char* BLACK = "BLACK";
constexpr const char* BRIGHT_RED = "BRIGHT_RED";
constexpr const char* BRIGHT_GREEN = "BRIGHT_GREEN";
constexpr const char* BRIGHT_BLUE = "BRIGHT_BLUE";
constexpr const char* BRIGHT_YELLOW = "BRIGHT_YELLOW";
constexpr const char* BRIGHT_CYAN = "BRIGHT_CYAN";
constexpr const char* BRIGHT_MAGENTA = "BRIGHT_MAGENTA";
constexpr const char* BRIGHT_WHITE = "BRIGHT_WHITE";

// Returns a platform-neutral color ID (0-15)
uint16_t from_name(const std::string& color_name);

} // namespace guayaba::Color

#endif // GUAYABA_CORE_COLOR_HPP