#ifndef GUAYABA_CORE_TEXTWRAP_HPP
#define GUAYABA_CORE_TEXTWRAP_HPP

#include <string>
#include <vector>

#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

// Word-wraps `text` into lines of at most `max_width` cells. A cell is one
// codepoint on the monospace grid, so widths count codepoints (UTF-8 aware), not
// bytes.
//
// '\n' is a hard break. A word longer than max_width is not split; it is emitted
// whole and overflows the line. A non-positive max_width yields one line per hard
// break. Whitespace between words is preserved.
std::vector<std::string> wrap(const std::string& text, int max_width);

// The extent `text` wants if nothing constrains it: the width of its longest line
// and how many lines it takes. This is wrap() at unbounded width, a plain split
// on '\n', so the answer does not depend on the size the node is currently given.
Vec2 measure_text(const std::string& text);

} // namespace guayaba

#endif // GUAYABA_CORE_TEXTWRAP_HPP