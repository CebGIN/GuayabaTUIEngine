#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include <guayaba/Scene/SceneParser.hpp>
#include "SceneText.hpp"

#include "SceneIO.hpp"
#include "SceneLexer.hpp"
#include "SceneParserInternal.hpp"

namespace guayaba {

namespace detail {

namespace {

std::string token_description(const SceneToken& token) {
    switch (token.kind) {
        case SceneTokenKind::End: return "end of file";
        case SceneTokenKind::Identifier: return "identifier '" + token.text + "'";
        case SceneTokenKind::String: return "string";
        case SceneTokenKind::Integer: return "integer";
        case SceneTokenKind::Float: return "number";
        case SceneTokenKind::True:
        case SceneTokenKind::False: return "boolean";
        case SceneTokenKind::None: return "'none'";
        case SceneTokenKind::LeftBrace: return "'{'";
        case SceneTokenKind::RightBrace: return "'}'";
        case SceneTokenKind::LeftBracket: return "'['";
        case SceneTokenKind::RightBracket: return "']'";
        case SceneTokenKind::Equals: return "'='";
        case SceneTokenKind::Comma: return "','";
        case SceneTokenKind::Dot: return "'.'";
    }
    return "token";
}

} // namespace

ParserCore::ParserCore(SceneLexResult lexed, SceneParseOptions options)
    : tokens_(std::move(lexed.tokens)),
      source_(options.source_name.empty() ? "<memory>" : options.source_name),
      options_(std::move(options)),
      diagnostics_(std::move(lexed.diagnostics)) {
    if (tokens_.empty()) {
        tokens_.push_back(SceneToken{SceneTokenKind::End, "", 0, 0.0, {}});
    }
}

const SceneToken& ParserCore::peek(std::size_t ahead) const {
    const std::size_t index = std::min(index_ + ahead, tokens_.size() - 1);
    return tokens_[index];
}

const SceneToken& ParserCore::advance() {
    const SceneToken& token = peek();
    if (index_ + 1 < tokens_.size()) ++index_;
    return token;
}

bool ParserCore::check(SceneTokenKind kind) const {
    return peek().kind == kind;
}

bool ParserCore::match(SceneTokenKind kind) {
    if (!check(kind)) return false;
    advance();
    return true;
}

bool ParserCore::at_end() const {
    return check(SceneTokenKind::End);
}

bool ParserCore::expect(SceneTokenKind kind, std::string message, SceneToken* token) {
    if (!check(kind)) {
        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              std::move(message) + ", found " + token_description(peek()));
        return false;
    }
    const SceneToken& consumed = advance();
    if (token) *token = consumed;
    return true;
}

void ParserCore::error(SceneDiagnosticCode code, SceneSourceSpan span, std::string message) {
    diagnostics_.push_back(make_diagnostic(code, source_, span, std::move(message)));
}

SceneValue ParserCore::parse_value() {
    return parse_value_impl(0);
}

SceneValue ParserCore::parse_value_impl(int depth) {
    if (depth > static_cast<int>(options_.max_depth)) {
        error(SceneDiagnosticCode::ValueOutOfRange, peek().span,
              "maximum value nesting depth exceeded");
        advance();
        return SceneValue::null(peek().span);
    }

    const SceneToken& token = peek();
    const SceneSourceSpan span = token.span;

    switch (token.kind) {
        case SceneTokenKind::None:
            advance();
            return SceneValue::null(span);
        case SceneTokenKind::True:
            advance();
            return SceneValue::boolean(true, span);
        case SceneTokenKind::False:
            advance();
            return SceneValue::boolean(false, span);
        case SceneTokenKind::Integer:
            advance();
            return SceneValue::integer(token.int_value, span);
        case SceneTokenKind::Float:
            advance();
            return SceneValue::number(token.float_value, span);
        case SceneTokenKind::String:
            advance();
            return SceneValue::string(token.text, span);
        case SceneTokenKind::LeftBracket:
            return parse_array(depth + 1);
        case SceneTokenKind::LeftBrace:
            return parse_table(depth + 1);
        default:
            error(SceneDiagnosticCode::UnexpectedToken, span,
                  "expected a value, found " + token_description(token));
            advance();
            return SceneValue::null(span);
    }
}

SceneValue ParserCore::parse_array(int depth) {
    const SceneToken start = advance(); // '['
    std::vector<SceneValue> values;
    bool closed = false;

    if (match(SceneTokenKind::RightBracket)) {
        const SceneSourceSpan span{start.span.begin, peek().span.begin.offset - start.span.begin.offset};
        return SceneValue::array(std::move(values), span);
    }

    while (!at_end()) {
        values.push_back(parse_value_impl(depth));

        if (match(SceneTokenKind::Comma)) {
            if (match(SceneTokenKind::RightBracket)) {
                closed = true;
                break;
            }
            continue;
        }
        if (match(SceneTokenKind::RightBracket)) {
            closed = true;
            break;
        }

        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              "expected ',' or ']' after array element, found " + token_description(peek()));
        while (!at_end() && !check(SceneTokenKind::Comma) && !check(SceneTokenKind::RightBracket)) {
            advance();
        }
        if (match(SceneTokenKind::Comma)) {
            if (match(SceneTokenKind::RightBracket)) {
                closed = true;
                break;
            }
            continue;
        }
        if (match(SceneTokenKind::RightBracket)) closed = true;
        break;
    }

    if (!closed) {
        error(SceneDiagnosticCode::UnexpectedToken, start.span,
              "unterminated array");
    }
    const SceneSourceSpan span{start.span.begin, peek().span.begin.offset - start.span.begin.offset};
    return SceneValue::array(std::move(values), span);
}

SceneValue ParserCore::parse_table(int depth) {
    const SceneToken start = advance(); // '{'
    std::vector<SceneNamedValue> members;
    std::set<std::string> names;
    bool closed = false;

    if (match(SceneTokenKind::RightBrace)) {
        const SceneSourceSpan span{start.span.begin, peek().span.begin.offset - start.span.begin.offset};
        return SceneValue::object(std::move(members), span);
    }

    while (!at_end()) {
        SceneSourceSpan key_span;
        const std::string key = parse_key(&key_span);
        if (key.empty()) {
            recover_statement();
            if (match(SceneTokenKind::RightBrace)) break;
            continue;
        }

        if (!expect(SceneTokenKind::Equals, "expected '=' after table key '" + key + "'")) {
            recover_statement();
            if (match(SceneTokenKind::RightBrace)) break;
            continue;
        }

        SceneValue value = parse_value_impl(depth);
        if (!names.insert(key).second) {
            error(SceneDiagnosticCode::DuplicateProperty, key_span,
                  "duplicate table field '" + key + "'");
        }
        members.push_back(SceneNamedValue{key, std::move(value), key_span});

        if (match(SceneTokenKind::Comma)) {
            if (match(SceneTokenKind::RightBrace)) {
                closed = true;
                break;
            }
            continue;
        }
        if (match(SceneTokenKind::RightBrace)) {
            closed = true;
            break;
        }

        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              "expected ',' or '}' after table field, found " + token_description(peek()));
        recover_statement();
        if (match(SceneTokenKind::RightBrace)) {
            closed = true;
            break;
        }
    }

    if (!closed) {
        error(SceneDiagnosticCode::UnexpectedToken, start.span,
              "unterminated table");
    }
    const SceneSourceSpan span{start.span.begin, peek().span.begin.offset - start.span.begin.offset};
    return SceneValue::object(std::move(members), span);
}

std::string ParserCore::parse_key(SceneSourceSpan* span) {
    if (!check(SceneTokenKind::Identifier)) {
        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              "expected an identifier, found " + token_description(peek()));
        return {};
    }

    const SceneSourceLocation start = peek().span.begin;
    std::string key = advance().text;
    while (match(SceneTokenKind::Dot)) {
        if (!check(SceneTokenKind::Identifier)) {
            error(SceneDiagnosticCode::UnexpectedToken, peek().span,
                  "expected an identifier after '.', found " + token_description(peek()));
            break;
        }
        key += '.';
        key += advance().text;
    }

    if (span) {
        *span = SceneSourceSpan{start, peek().span.begin.offset - start.offset};
    }
    return key;
}

void ParserCore::recover_statement() {
    const std::uint32_t line = peek().span.begin.line;
    while (!at_end() && !check(SceneTokenKind::RightBrace)) {
        if (check(SceneTokenKind::Identifier) && peek().span.begin.line > line) break;
        advance();
    }
}

void ParserCore::skip_balanced_block() {
    if (!check(SceneTokenKind::LeftBrace)) {
        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              "expected '{' to start a node block");
        return;
    }

    advance();
    int depth = 1;
    while (!at_end() && depth > 0) {
        if (check(SceneTokenKind::LeftBrace)) ++depth;
        else if (check(SceneTokenKind::RightBrace)) --depth;
        advance();
    }

    if (depth > 0) {
        error(SceneDiagnosticCode::UnexpectedToken, peek().span,
              "unterminated node block");
    }
}

} // namespace detail

namespace {

using detail::ParserCore;
using detail::SceneToken;
using detail::SceneTokenKind;

// Top-level .gscene structure: header, one root node, and the diagnostics that
// keep a malformed file editable instead of throwing at the first token.
class SceneParser {
public:
    explicit SceneParser(detail::SceneLexResult lexed, SceneParseOptions options)
        : core_(std::move(lexed), std::move(options)) {}

    SceneParseResult parse() {
        parse_header();
        parse_root();
        skip_trailing();

        SceneParseResult result;
        result.document = std::move(document_);
        result.diagnostics = core_.take_diagnostics();
        return result;
    }

private:
    bool is_header_key(const std::string& key) const {
        return key == "format" || key == "name" || key == "theme";
    }

    void parse_header() {
        while (core_.check(SceneTokenKind::Identifier) &&
               core_.peek(1).kind == SceneTokenKind::Equals) {
            const SceneToken key_token = core_.advance();
            core_.advance(); // '='
            const std::string key = key_token.text;
            const SceneValue value = core_.parse_value();

            if (!is_header_key(key)) {
                core_.error(SceneDiagnosticCode::UnexpectedToken, key_token.span,
                            "unknown header key '" + key + "'");
                continue;
            }
            if (!seen_headers_.insert(key).second) {
                core_.error(SceneDiagnosticCode::DuplicateHeaderKey, key_token.span,
                            "duplicate header key '" + key + "'");
                continue;
            }

            if (key == "format") {
                if (value.kind() != SceneValueKind::Int) {
                    core_.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                                "header 'format' must be an integer");
                    continue;
                }
                const long long version = value.as_int();
                if (version != 1) {
                    core_.error(SceneDiagnosticCode::UnsupportedVersion, value.span(),
                                "unsupported scene format version " +
                                    std::to_string(version) + "; this build supports 1");
                } else {
                    document_.format_version = 1;
                }
            } else if (key == "name") {
                if (value.kind() != SceneValueKind::String) {
                    core_.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                                "header 'name' must be a string");
                    continue;
                }
                document_.name = value.as_string();
            } else if (key == "theme") {
                if (value.kind() != SceneValueKind::String) {
                    core_.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                                "header 'theme' must be a string");
                    continue;
                }
                document_.theme = value.as_string();
            }
        }

        if (seen_headers_.find("format") == seen_headers_.end()) {
            core_.error(SceneDiagnosticCode::MissingFormat, core_.peek().span,
                        "missing required header 'format = 1'");
        }
    }

    void parse_root() {
        if (!core_.check(SceneTokenKind::Identifier) ||
            core_.peek(1).kind != SceneTokenKind::String) {
            core_.error(SceneDiagnosticCode::MissingRoot, core_.peek().span,
                        "expected the root node: Type \"Name\" { ... }");
            return;
        }

        document_.root = parse_node(0);
    }

    void skip_trailing() {
        while (!core_.at_end()) {
            if (core_.check(SceneTokenKind::Identifier) &&
                core_.peek(1).kind == SceneTokenKind::String) {
                core_.error(SceneDiagnosticCode::MultipleRoots, core_.peek().span,
                            "the document has more than one root node");
                skip_node();
                continue;
            }

            if (core_.check(SceneTokenKind::Identifier) &&
                core_.peek(1).kind == SceneTokenKind::Equals) {
                core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                            "header keys must appear before the root node");
                core_.advance();
                core_.advance();
                (void)core_.parse_value();
                continue;
            }

            core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                        "unexpected content after the root node");
            core_.advance();
            core_.recover_statement();
        }
    }

    void skip_node() {
        core_.advance(); // type
        if (core_.check(SceneTokenKind::String)) core_.advance();
        if (core_.check(SceneTokenKind::LeftBrace)) {
            core_.skip_balanced_block();
        } else {
            core_.recover_statement();
        }
    }

    std::unique_ptr<SceneNodeDesc> parse_node(int depth) {
        if (depth > static_cast<int>(core_.options().max_depth)) {
            core_.error(SceneDiagnosticCode::ValueOutOfRange, core_.peek().span,
                        "maximum node nesting depth exceeded");
            skip_node();
            return nullptr;
        }
        if (node_count_ >= core_.options().max_nodes) {
            core_.error(SceneDiagnosticCode::ValueOutOfRange, core_.peek().span,
                        "maximum node count exceeded");
            skip_node();
            return nullptr;
        }

        const SceneToken type_token = core_.advance();
        SceneToken name_token;
        if (!core_.expect(SceneTokenKind::String, "expected a node name string", &name_token)) {
            core_.recover_statement();
            return nullptr;
        }

        std::string name_error;
        if (!detail::is_valid_node_name(name_token.text, &name_error)) {
            core_.error(SceneDiagnosticCode::InvalidName, name_token.span,
                        "invalid node name: " + name_error);
        }

        if (!core_.expect(SceneTokenKind::LeftBrace, "expected '{' after the node name")) {
            core_.recover_statement();
            return nullptr;
        }

        ++node_count_;
        auto node = std::make_unique<SceneNodeDesc>();
        node->type = type_token.text;
        node->name = name_token.text;
        node->span = type_token.span;

        std::set<std::string> property_names;
        std::set<std::string> child_names;
        bool closed = false;

        while (!core_.at_end()) {
            if (core_.match(SceneTokenKind::RightBrace)) {
                closed = true;
                break;
            }

            if (!core_.check(SceneTokenKind::Identifier)) {
                core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                            "expected a property or child node, found " +
                                detail::token_description(core_.peek()));
                core_.advance();
                core_.recover_statement();
                continue;
            }

            const bool is_child = core_.peek(1).kind == SceneTokenKind::String;
            const bool is_property = core_.peek(1).kind == SceneTokenKind::Equals ||
                                     core_.peek(1).kind == SceneTokenKind::Dot;

            if (is_child) {
                std::unique_ptr<SceneNodeDesc> child = parse_node(depth + 1);
                if (child) {
                    if (!child_names.insert(child->name).second) {
                        core_.error(SceneDiagnosticCode::DuplicateSibling, child->span,
                                    "duplicate sibling name '" + child->name + "'");
                    }
                    node->children.push_back(std::move(child));
                }
            } else if (is_property) {
                parse_property(*node, property_names);
            } else {
                core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                            "expected '=' after property name, or a name string for a child node");
                core_.advance();
                core_.recover_statement();
            }
        }

        if (!closed) {
            core_.error(SceneDiagnosticCode::UnexpectedToken, type_token.span,
                        "unterminated node block for '" + node->name + "'");
        }

        node->span = SceneSourceSpan{type_token.span.begin,
                                     core_.peek().span.begin.offset - type_token.span.begin.offset};
        return node;
    }

    void parse_property(SceneNodeDesc& node, std::set<std::string>& property_names) {
        SceneSourceSpan key_span;
        const std::string key = core_.parse_key(&key_span);
        if (key.empty()) {
            core_.recover_statement();
            return;
        }

        if (!core_.expect(SceneTokenKind::Equals, "expected '=' after property '" + key + "'")) {
            core_.recover_statement();
            return;
        }

        const SceneValue value = core_.parse_value();
        if (!property_names.insert(key).second) {
            core_.error(SceneDiagnosticCode::DuplicateProperty, key_span,
                        "duplicate property '" + key + "'");
        }

        node.properties.push_back(SceneProperty{key, value, key_span});
    }

    ParserCore core_;
    SceneDocument document_;
    std::set<std::string> seen_headers_;
    std::size_t node_count_ = 0;
};


} // namespace

SceneParseResult parse_scene(std::string_view text, const SceneParseOptions& options) {
    SceneParseOptions normalized = options;
    if (normalized.source_name.empty()) normalized.source_name = "<memory>";
    if (text.size() > normalized.max_bytes) {
        SceneParseResult result;
        result.diagnostics.push_back(make_diagnostic(
            SceneDiagnosticCode::IoError, normalized.source_name, {},
            "file is larger than the configured maximum (" +
                std::to_string(normalized.max_bytes) + " bytes)"));
        return result;
    }

    detail::SceneLexResult lexed = detail::lex_scene(text, normalized.source_name);
    SceneParser parser(std::move(lexed), std::move(normalized));
    return parser.parse();
}

SceneParseResult parse_scene_file(const std::filesystem::path& path,
                                  const SceneParseOptions& options) {
    SceneParseOptions normalized = options;
    normalized.source_name = path.string();

    detail::TextFile file = detail::read_text_file(path, normalized.max_bytes);
    if (!file.ok) {
        SceneParseResult result;
        result.diagnostics.push_back(make_diagnostic(
            SceneDiagnosticCode::IoError, normalized.source_name, {}, file.error));
        return result;
    }

    return parse_scene(file.text, normalized);
}

} // namespace guayaba
