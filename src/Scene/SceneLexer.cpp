#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include <guayaba/Core/Utf8.hpp>
#include "SceneLexer.hpp"
#include "SceneText.hpp"

namespace guayaba::detail {

namespace {

bool is_identifier_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool is_identifier_continue(char c) {
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void append_utf8(std::string& out, char32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

SceneSourceLocation location_at(std::string_view text, std::size_t offset) {
    SceneSourceLocation location;
    const std::string copy(text);
    std::size_t index = 0;
    while (index < offset && index < copy.size()) {
        const unsigned char lead = static_cast<unsigned char>(copy[index]);
        if (lead == '\r') {
            ++index;
            if (index < copy.size() && copy[index] == '\n') ++index;
            ++location.line;
            location.column = 1;
            continue;
        }
        if (lead == '\n') {
            ++index;
            ++location.line;
            location.column = 1;
            continue;
        }

        char32_t codepoint = 0;
        const std::size_t length = decode_utf8(copy, index, codepoint);
        index += length;
        ++location.column;
    }
    location.offset = offset;
    return location;
}

class Lexer {
public:
    Lexer(std::string_view text, std::string source)
        : text_(text), source_(std::move(source)) {}

    SceneLexResult run() {
        if (!is_valid_utf8(text_, &invalid_offset_)) {
            SceneSourceLocation location = location_at(text_, invalid_offset_);
            SceneSourceSpan span{location, 1};
            diagnostics_.push_back(make_diagnostic(
                SceneDiagnosticCode::InvalidUtf8, source_, span,
                "the file is not valid UTF-8"));
            tokens_.push_back(SceneToken{SceneTokenKind::End, "", 0, 0.0, span});
            return {std::move(tokens_), std::move(diagnostics_)};
        }

        // A UTF-8 BOM is tolerated and ignored.
        if (text_.size() >= 3 &&
            static_cast<unsigned char>(text_[0]) == 0xEF &&
            static_cast<unsigned char>(text_[1]) == 0xBB &&
            static_cast<unsigned char>(text_[2]) == 0xBF) {
            offset_ = 3;
        }

        while (true) {
            skip_space_and_comments();
            if (at_end()) break;

            const std::size_t start = offset_;
            const SceneSourceLocation start_location = location();
            const char c = text_[offset_];

            if (is_identifier_start(c)) {
                lex_identifier(start, start_location);
            } else if (is_digit(c) || (c == '-' && offset_ + 1 < text_.size() &&
                                       is_digit(text_[offset_ + 1]))) {
                lex_number(start, start_location);
            } else if (c == '"') {
                lex_string(start, start_location);
            } else {
                lex_punctuation(start, start_location);
            }
        }

        const SceneSourceLocation end = location();
        tokens_.push_back(SceneToken{SceneTokenKind::End, "", 0, 0.0, {end, 0}});
        return {std::move(tokens_), std::move(diagnostics_)};
    }

private:
    bool at_end() const { return offset_ >= text_.size(); }

    char peek(std::size_t ahead = 0) const {
        const std::size_t index = offset_ + ahead;
        return index < text_.size() ? text_[index] : '\0';
    }

    SceneSourceLocation location() const {
        return SceneSourceLocation{offset_, line_, column_};
    }

    void advance() {
        if (at_end()) return;

        const unsigned char lead = static_cast<unsigned char>(text_[offset_]);
        if (lead == '\r') {
            ++offset_;
            if (!at_end() && text_[offset_] == '\n') ++offset_;
            ++line_;
            column_ = 1;
            return;
        }
        if (lead == '\n') {
            ++offset_;
            ++line_;
            column_ = 1;
            return;
        }

        if (lead <= 0x7F) {
            ++offset_;
            ++column_;
            return;
        }

        const std::string copy(text_);
        char32_t codepoint = 0;
        const std::size_t length = decode_utf8(copy, offset_, codepoint);
        offset_ += length;
        ++column_;
    }

    void advance_ascii(std::size_t count = 1) {
        for (std::size_t i = 0; i < count && !at_end(); ++i) {
            ++offset_;
            ++column_;
        }
    }

    SceneSourceSpan span_from(std::size_t start, SceneSourceLocation start_location) const {
        return SceneSourceSpan{start_location, offset_ - start};
    }

    void error(SceneDiagnosticCode code, SceneSourceSpan span, std::string message) {
        diagnostics_.push_back(make_diagnostic(code, source_, span, std::move(message)));
    }

    void skip_space_and_comments() {
        while (!at_end()) {
            const char c = text_[offset_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
                continue;
            }
            if (c == '#') {
                while (!at_end() && text_[offset_] != '\n' && text_[offset_] != '\r') {
                    advance();
                }
                continue;
            }
            break;
        }
    }

    void lex_identifier(std::size_t start, SceneSourceLocation start_location) {
        while (!at_end() && is_identifier_continue(text_[offset_])) advance_ascii();

        const std::string text(text_.substr(start, offset_ - start));
        SceneTokenKind kind = SceneTokenKind::Identifier;
        if (text == "true") kind = SceneTokenKind::True;
        else if (text == "false") kind = SceneTokenKind::False;
        else if (text == "none") kind = SceneTokenKind::None;

        tokens_.push_back(SceneToken{kind, text, 0, 0.0,
                                     span_from(start, start_location)});
    }

    void lex_number(std::size_t start, SceneSourceLocation start_location) {
        bool is_float = false;
        if (peek() == '-') advance_ascii();

        while (is_digit(peek())) advance_ascii();

        if (peek() == '.' && is_digit(peek(1))) {
            is_float = true;
            advance_ascii();
            while (is_digit(peek())) advance_ascii();
        }

        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance_ascii();
            if (peek() == '+' || peek() == '-') advance_ascii();
            if (!is_digit(peek())) {
                error(SceneDiagnosticCode::InvalidNumber, span_from(start, start_location),
                      "exponent has no digits");
                return;
            }
            while (is_digit(peek())) advance_ascii();
        }

        // A second '.', an identifier immediately after a number, or a stray '-'
        // is not part of the token and will be diagnosed by the parser.
        const std::string text(text_.substr(start, offset_ - start));
        const char* begin = text.data();
        const char* end = begin + text.size();

        if (is_float) {
            double value = 0.0;
            const auto result = std::from_chars(begin, end, value, std::chars_format::general);
            if (result.ec != std::errc() || result.ptr != end) {
                error(SceneDiagnosticCode::InvalidNumber, span_from(start, start_location),
                      "invalid floating-point number '" + text + "'");
            }
            tokens_.push_back(SceneToken{SceneTokenKind::Float, text, 0, value,
                                         span_from(start, start_location)});
            return;
        }

        long long value = 0;
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end) {
            error(SceneDiagnosticCode::InvalidNumber, span_from(start, start_location),
                  "invalid integer '" + text + "'");
        }
        tokens_.push_back(SceneToken{SceneTokenKind::Integer, text, value, 0.0,
                                     span_from(start, start_location)});
    }

    void append_escape(std::string& out, std::size_t string_start,
                       SceneSourceLocation string_location) {
        // The backslash has already been consumed.
        if (at_end()) {
            error(SceneDiagnosticCode::UnterminatedString,
                  span_from(string_start, string_location),
                  "unterminated string literal");
            return;
        }

        const char escape = text_[offset_];
        advance_ascii();
        switch (escape) {
            case '\\': out.push_back('\\'); return;
            case '"': out.push_back('"'); return;
            case 'n': out.push_back('\n'); return;
            case 'r': out.push_back('\r'); return;
            case 't': out.push_back('\t'); return;
            case 'u':
            case 'U': {
                const std::size_t digits = escape == 'u' ? 4 : 8;
                char32_t codepoint = 0;
                for (std::size_t i = 0; i < digits; ++i) {
                    if (at_end()) {
                        error(SceneDiagnosticCode::InvalidEscape,
                              span_from(string_start, string_location),
                              "incomplete Unicode escape");
                        return;
                    }
                    const int value = hex_value(text_[offset_]);
                    if (value < 0) {
                        error(SceneDiagnosticCode::InvalidEscape,
                              span_from(string_start, string_location),
                              "invalid Unicode escape");
                        advance();
                        return;
                    }
                    codepoint = (codepoint << 4) | static_cast<char32_t>(value);
                    advance_ascii();
                }

                if (codepoint > 0x10FFFF ||
                    (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                    error(SceneDiagnosticCode::InvalidEscape,
                          span_from(string_start, string_location),
                          "Unicode escape is not a valid codepoint");
                    return;
                }
                append_utf8(out, codepoint);
                return;
            }
            default:
                error(SceneDiagnosticCode::InvalidEscape,
                      span_from(string_start, string_location),
                      std::string("unknown escape sequence '\\") + escape + "'");
                out.push_back(escape);
                return;
        }
    }

    void lex_string(std::size_t start, SceneSourceLocation start_location) {
        advance_ascii(); // opening quote
        std::string decoded;

        while (!at_end()) {
            const char c = text_[offset_];
            if (c == '"') {
                advance_ascii();
                tokens_.push_back(SceneToken{SceneTokenKind::String, std::move(decoded),
                                             0, 0.0, span_from(start, start_location)});
                return;
            }
            if (c == '\n' || c == '\r') {
                error(SceneDiagnosticCode::UnterminatedString,
                      span_from(start, start_location),
                      "unterminated string literal");
                tokens_.push_back(SceneToken{SceneTokenKind::String, std::move(decoded),
                                             0, 0.0, span_from(start, start_location)});
                return;
            }
            if (c == '\\') {
                advance_ascii();
                append_escape(decoded, start, start_location);
                continue;
            }

            const unsigned char byte = static_cast<unsigned char>(c);
            if (byte < 0x20 && c != '\t') {
                error(SceneDiagnosticCode::InvalidEscape,
                      span_from(start, start_location),
                      "raw control character in string literal");
                advance();
                continue;
            }

            if (byte <= 0x7F) {
                decoded.push_back(c);
                advance_ascii();
            } else {
                const std::string copy(text_);
                char32_t codepoint = 0;
                const std::size_t length = decode_utf8(copy, offset_, codepoint);
                decoded.append(text_.data() + offset_, length);
                advance();
            }
        }

        error(SceneDiagnosticCode::UnterminatedString,
              span_from(start, start_location),
              "unterminated string literal");
        tokens_.push_back(SceneToken{SceneTokenKind::String, std::move(decoded),
                                     0, 0.0, span_from(start, start_location)});
    }

    void lex_punctuation(std::size_t start, SceneSourceLocation start_location) {
        SceneTokenKind kind = SceneTokenKind::End;
        switch (text_[offset_]) {
            case '{': kind = SceneTokenKind::LeftBrace; break;
            case '}': kind = SceneTokenKind::RightBrace; break;
            case '[': kind = SceneTokenKind::LeftBracket; break;
            case ']': kind = SceneTokenKind::RightBracket; break;
            case '=': kind = SceneTokenKind::Equals; break;
            case ',': kind = SceneTokenKind::Comma; break;
            case '.': kind = SceneTokenKind::Dot; break;
            default:
                error(SceneDiagnosticCode::UnexpectedToken,
                      SceneSourceSpan{start_location, 1},
                      std::string("unexpected character '") + text_[offset_] + "'");
                advance();
                return;
        }

        advance_ascii();
        tokens_.push_back(SceneToken{kind, std::string(1, text_[start]), 0, 0.0,
                                     span_from(start, start_location)});
    }

    std::string_view text_;
    std::string source_;
    std::size_t offset_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;
    std::size_t invalid_offset_ = 0;
    std::vector<SceneToken> tokens_;
    std::vector<SceneDiagnostic> diagnostics_;
};

} // namespace

SceneLexResult lex_scene(std::string_view text, const std::string& source) {
    return Lexer(text, source).run();
}

} // namespace guayaba::detail
