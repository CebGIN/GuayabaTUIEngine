#include <guayaba/Core/Color.hpp>

namespace guayaba::Color {

uint16_t from_name(const std::string& color_name) {
    if (color_name == "BLACK")          return 0;
    if (color_name == "RED")            return 1;
    if (color_name == "GREEN")          return 2;
    if (color_name == "YELLOW")         return 3;
    if (color_name == "BLUE")           return 4;
    if (color_name == "MAGENTA")        return 5;
    if (color_name == "CYAN")           return 6;
    if (color_name == "WHITE")          return 7;
    if (color_name == "BRIGHT_BLACK")   return 8;
    if (color_name == "BRIGHT_RED")     return 9;
    if (color_name == "BRIGHT_GREEN")   return 10;
    if (color_name == "BRIGHT_BLUE")    return 12;
    if (color_name == "BRIGHT_YELLOW")  return 11;
    if (color_name == "BRIGHT_CYAN")    return 14;
    if (color_name == "BRIGHT_MAGENTA") return 13;
    if (color_name == "BRIGHT_WHITE")   return 15;
return 7; // Default: white
}

} // namespace guayaba::Color