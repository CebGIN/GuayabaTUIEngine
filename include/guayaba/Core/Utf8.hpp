#ifndef GUAYABA_CORE_UTF8_HPP
#define GUAYABA_CORE_UTF8_HPP

#include <cstddef>
#include <string>

namespace guayaba {

// Decodes the UTF-8 codepoint that starts at byte `index` in `value`, writing it
// to `codepoint` and returning how many bytes it spans.
//
// A truncated or malformed sequence is reported as a single byte (the lead byte,
// taken as-is), so a bad string still draws one cell instead of derailing the
// rest of the line. Callers advance by the returned length, so a malformed byte
// can never walk them off the end of the string.
//
// The engine's grid is monospace: one codepoint is one cell, whatever its UTF-8
// length. Every place that walks text - the renderer, the wrapping, and the list
// and field controls - has to agree on these boundaries, so the decoder lives
// here rather than being copied into each translation unit. Measuring or drawing
// by byte instead is what punches holes into any line holding an accent, an
// arrow or a box glyph.
inline std::size_t decode_utf8(const std::string& value, std::size_t index,
                               char32_t& codepoint) {
    const unsigned char lead = static_cast<unsigned char>(value[index]);

    std::size_t length = 1;
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
    }

    if (index + length > value.size()) {
        codepoint = lead;
        return 1;
    }

    char32_t decoded = (length == 1) ? lead
                     : (length == 2) ? (lead & 0x1F)
                     : (length == 3) ? (lead & 0x0F)
                                     : (lead & 0x07);
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char continuation =
            static_cast<unsigned char>(value[index + i]);
        if ((continuation & 0xC0) != 0x80) {
            codepoint = lead;  // malformed: emit the lead alone
            return 1;
        }
        decoded = (decoded << 6) | (continuation & 0x3F);
    }

    codepoint = decoded;
    return length;
}

// How many cells `text` occupies on the grid: one per codepoint, not per byte.
inline int text_width(const std::string& text) {
    int cells = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        char32_t codepoint = 0;
        index += decode_utf8(text, index, codepoint);
        ++cells;
    }
    return cells;
}

}  // namespace guayaba

#endif  // GUAYABA_CORE_UTF8_HPP
