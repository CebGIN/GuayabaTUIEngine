#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>

#include <guayaba/Core/Utf8.hpp>
#include "SceneText.hpp"

namespace guayaba::detail {

namespace {

struct ColorName {
    std::uint16_t value;
    const char* name;
};

constexpr std::array<ColorName, 16> COLOR_NAMES{{
    {0, "BLACK"},
    {1, "RED"},
    {2, "GREEN"},
    {3, "YELLOW"},
    {4, "BLUE"},
    {5, "MAGENTA"},
    {6, "CYAN"},
    {7, "WHITE"},
    {8, "BRIGHT_BLACK"},
    {9, "BRIGHT_RED"},
    {10, "BRIGHT_GREEN"},
    {11, "BRIGHT_YELLOW"},
    {12, "BRIGHT_BLUE"},
    {13, "BRIGHT_MAGENTA"},
    {14, "BRIGHT_CYAN"},
    {15, "BRIGHT_WHITE"},
}};

std::string upper_ascii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        out.push_back(c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
    }
    return out;
}

char hex_digit(unsigned value) {
    return static_cast<char>(value < 10 ? '0' + value : 'A' + (value - 10));
}

void append_unicode_escape(std::string& out, unsigned codepoint) {
    out += "\\u";
    out.push_back(hex_digit((codepoint >> 12) & 0xF));
    out.push_back(hex_digit((codepoint >> 8) & 0xF));
    out.push_back(hex_digit((codepoint >> 4) & 0xF));
    out.push_back(hex_digit(codepoint & 0xF));
}

} // namespace

bool is_valid_utf8(std::string_view text, std::size_t* bad_offset) {
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 0;
        unsigned minimum = 0;

        if (lead <= 0x7F) {
            length = 1;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            minimum = 0x80;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            minimum = 0x800;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            minimum = 0x10000;
        } else {
            if (bad_offset) *bad_offset = i;
            return false;
        }

        if (i + length > text.size()) {
            if (bad_offset) *bad_offset = i;
            return false;
        }

        unsigned codepoint = length == 1 ? lead : (lead & (0xFF >> (length + 1)));
        for (std::size_t j = 1; j < length; ++j) {
            const unsigned char continuation = static_cast<unsigned char>(text[i + j]);
            if ((continuation & 0xC0) != 0x80) {
                if (bad_offset) *bad_offset = i + j;
                return false;
            }
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }

        if (length > 1 && codepoint < minimum) {
            if (bad_offset) *bad_offset = i;
            return false;
        }
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            if (bad_offset) *bad_offset = i;
            return false;
        }

        i += length;
    }
    return true;
}

std::size_t utf8_codepoint_count(std::string_view text) {
    const std::string copy(text);
    std::size_t index = 0;
    std::size_t count = 0;
    while (index < copy.size()) {
        char32_t codepoint = 0;
        index += decode_utf8(copy, index, codepoint);
        ++count;
    }
    return count;
}

bool is_single_printable_glyph(std::string_view text, char32_t* codepoint) {
    if (text.empty()) return false;

    const std::string copy(text);
    char32_t decoded = 0;
    const std::size_t length = decode_utf8(copy, 0, decoded);
    if (length != text.size()) return false;
    if (decoded < 0x20 || decoded == 0x7F) return false;

    if (codepoint) *codepoint = decoded;
    return true;
}

std::size_t byte_offset_for_codepoint(std::string_view text, std::size_t index) {
    const std::string copy(text);
    std::size_t byte = 0;
    std::size_t codepoint_index = 0;
    while (byte < copy.size() && codepoint_index < index) {
        char32_t codepoint = 0;
        const std::size_t length = decode_utf8(copy, byte, codepoint);
        byte += length;
        ++codepoint_index;
    }
    return byte;
}

bool is_valid_node_name(std::string_view name, std::string* error) {
    if (name.empty()) {
        if (error) *error = "node name cannot be empty";
        return false;
    }
    if (!is_valid_utf8(name)) {
        if (error) *error = "node name is not valid UTF-8";
        return false;
    }
    if (name.find('/') != std::string_view::npos) {
        if (error) *error = "node name cannot contain '/'";
        return false;
    }

    const std::string copy(name);
    std::size_t index = 0;
    while (index < copy.size()) {
        char32_t codepoint = 0;
        index += decode_utf8(copy, index, codepoint);
        if (codepoint < 0x20 || codepoint == 0x7F) {
            if (error) *error = "node name cannot contain control characters";
            return false;
        }
    }
    return true;
}

bool has_line_break(std::string_view text) {
    return text.find('\n') != std::string_view::npos ||
           text.find('\r') != std::string_view::npos;
}

std::string canonical_color_name(std::uint16_t color) {
    for (const ColorName& entry : COLOR_NAMES) {
        if (entry.value == color) return entry.name;
    }
    return "WHITE";
}

bool parse_color_name(std::string_view name, std::uint16_t& color) {
    const std::string upper = upper_ascii(name);
    for (const ColorName& entry : COLOR_NAMES) {
        if (upper == entry.name) {
            color = entry.value;
            return true;
        }
    }
    return false;
}

std::string format_float(double value) {
    if (!std::isfinite(value)) return "0.0";
    if (value == 0.0) return "0.0";

    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                      std::chars_format::general);
    if (result.ec != std::errc()) {
        std::ostringstream fallback;
        fallback.imbue(std::locale::classic());
        fallback << std::setprecision(17) << value;
        return fallback.str();
    }

    std::string out(buffer, result.ptr);
    if (out.find('.') == std::string::npos &&
        out.find('e') == std::string::npos &&
        out.find('E') == std::string::npos) {
        out += ".0";
    }
    return out;
}

std::string escape_string(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (byte < 0x20 || byte == 0x7F) {
                    append_unicode_escape(out, byte);
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

} // namespace guayaba::detail
