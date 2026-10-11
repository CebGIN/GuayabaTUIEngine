#ifndef GUAYABA_CORE_TEXTWRAP_HPP
#define GUAYABA_CORE_TEXTWRAP_HPP

#include <string>
#include <vector>

#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

// Word-wraps `text` into lines of at most `max_width` cells.
//
// This is pure string logic and deliberately knows nothing about the renderer,
// about layout, or about what a "cell" is: on a monospace grid a character's
// width is 1, so measuring is just string length. That keeps wrapping testable
// on its own and keeps the renderer free of text semantics.
//
// '\n' is a hard break, so a paragraph structure in the source text survives.
// A word longer than max_width is NOT split - it is emitted whole and overflows
// the line, leaving the caller to decide whether an ancestor clip hides it. A
// non-positive max_width yields one line per hard break rather than looping or
// discarding text.
//
// Preserves leading/trailing spaces on the words it keeps, so a wrap is a
// lossless re-partitioning of the input apart from the '\n' separators.
std::vector<std::string> wrap(const std::string& text, int max_width);

// The extent `text` wants if nothing constrains it: the width of its longest
// line, and how many lines it takes.
//
// This is wrap() at an unbounded width, which is a plain split on '\n'. It is
// the size a node has to reserve for its text, and it is deliberately NOT the
// current wrap: a minimum has to answer the same question whatever size the node
// happens to be sitting at, so measuring the wrap would let the layout decide
// the answer - a label squeezed narrow would report the short lines it was
// forced into, and the same text would report a different size in a different
// cell.
Vec2 measure_text(const std::string& text);

} // namespace guayaba

#endif // GUAYABA_CORE_TEXTWRAP_HPP