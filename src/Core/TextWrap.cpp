#include <algorithm>
#include <limits>

#include <guayaba/Core/TextWrap.hpp>
#include <guayaba/Core/Utf8.hpp>

namespace guayaba {

namespace {

// End of the word starting at `from`, and where the following whitespace run
// begins. Returns text.size() when the word runs to the end of the paragraph.
void next_word(const std::string& text, size_t from, size_t& word_end, size_t& gap_end) {
    size_t i = from;
    while (i < text.size() && text[i] != ' ' && text[i] != '\t') ++i;
    word_end = i;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    gap_end = i;
}

void wrap_paragraph(const std::string& text, int max_width, std::vector<std::string>& out) {
    // Start a fresh line for this paragraph. Whitespace runs between words are
    // re-emitted as the single space that joins them.
    out.push_back(std::string());

    size_t pos = 0;
    while (pos < text.size()) {
        size_t word_end = pos;
        size_t gap_end = pos;
        next_word(text, pos, word_end, gap_end);
        if (word_end == pos) {  // leading whitespace only
            pos = gap_end;
            continue;
        }

        const std::string word = text.substr(pos, word_end - pos);
        const int width = text_width(out.back());
        const int needed = width + (width > 0 ? 1 : 0) + text_width(word);

        // A word wider than max_width is not split; it goes out whole and
        // overflows. It shares a line only if that line is empty.
        if (width > 0 && max_width > 0 && needed > max_width) {
            out.push_back(word);
        } else {
            if (width > 0) out.back() += ' ';
            out.back() += word;
        }

        pos = gap_end;
    }
}

} // namespace

std::vector<std::string> wrap(const std::string& text, int max_width) {
    std::vector<std::string> out;

    // Split on hard breaks, then wrap each paragraph independently. A paragraph
    // that wraps to nothing still gets its line, so blank lines survive.
    size_t start = 0;
    while (true) {
        const size_t nl = text.find('\n', start);
        const std::string paragraph =
            text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);

        const size_t before = out.size();
        wrap_paragraph(paragraph, max_width, out);
        if (out.size() == before) out.push_back(std::string());

        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    return out;
}

Vec2 measure_text(const std::string& text) {
    // An unbounded width makes wrap() a plain split on '\n'.
    const std::vector<std::string> lines = wrap(text, std::numeric_limits<int>::max());

    int longest = 0;
    for (const std::string& line : lines) {
        longest = std::max(longest, text_width(line));
    }

    // Empty text still has one line, so a container does not collapse it away.
    return Vec2{longest, static_cast<int>(lines.size())};
}

} // namespace guayaba