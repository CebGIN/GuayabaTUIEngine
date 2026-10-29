#ifndef GUAYABA_SCENE_SCENETEXT_HPP
#define GUAYABA_SCENE_SCENETEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace guayaba::detail {

bool is_valid_utf8(std::string_view text, std::size_t* bad_offset = nullptr);

std::size_t utf8_codepoint_count(std::string_view text);

// True when `text` is exactly one printable codepoint: U+0020 or above, not DEL.
bool is_single_printable_glyph(std::string_view text, char32_t* codepoint = nullptr);

// Byte offset of the `index`-th codepoint. Returns text.size() when index is at
// or past the end.
std::size_t byte_offset_for_codepoint(std::string_view text, std::size_t index);

bool is_valid_node_name(std::string_view name, std::string* error = nullptr);
bool has_line_break(std::string_view text);

std::string canonical_color_name(std::uint16_t color);
bool parse_color_name(std::string_view name, std::uint16_t& color);

std::string format_float(double value);
std::string escape_string(std::string_view text);

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENETEXT_HPP
