#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>
#include <guayaba/Scene/SceneParser.hpp>
#include <guayaba/Scene/ThemeFormat.hpp>

#include "SceneFormatting.hpp"
#include "SceneIO.hpp"
#include "SceneLexer.hpp"
#include "SceneParserInternal.hpp"
#include "SceneText.hpp"
#include "SceneValueUtils.hpp"

namespace guayaba {

namespace {

using detail::ParserCore;
using detail::SceneToken;
using detail::SceneTokenKind;

const std::vector<std::string> STYLE_FIELDS{
    "background", "border", "border_style", "border_width", "corner_radius", "margin",
};

// Top-level .gtheme structure. It shares ParserCore's value grammar with the
// scene parser, but its entries are named StyleBoxes and colours.
class ThemeParser {
public:
    explicit ThemeParser(detail::SceneLexResult lexed, SceneParseOptions options)
        : core_(std::move(lexed), std::move(options)) {
        document_.source = core_.source();
    }

    ThemeParseResult parse() {
        parse_header();
        parse_entries();

        ThemeParseResult result;
        result.document = std::move(document_);
        result.diagnostics = core_.take_diagnostics();
        return result;
    }

private:
    void parse_header() {
        std::set<std::string> seen;
        while (core_.check(SceneTokenKind::Identifier) &&
               core_.peek(1).kind == SceneTokenKind::Equals) {
            const SceneToken key_token = core_.advance();
            core_.advance();
            const SceneValue value = core_.parse_value();

            if (key_token.text != "format" && key_token.text != "name") {
                core_.error(SceneDiagnosticCode::UnexpectedToken, key_token.span,
                            "unknown theme header key '" + key_token.text + "'");
                continue;
            }
            if (!seen.insert(key_token.text).second) {
                core_.error(SceneDiagnosticCode::DuplicateHeaderKey, key_token.span,
                            "duplicate theme header key '" + key_token.text + "'");
                continue;
            }

            if (key_token.text == "format") {
                if (value.kind() != SceneValueKind::Int) {
                    core_.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                                "header 'format' must be an integer");
                    continue;
                }
                if (value.as_int() != 1) {
                    core_.error(SceneDiagnosticCode::UnsupportedVersion, value.span(),
                                "unsupported theme format version " +
                                    std::to_string(value.as_int()) +
                                    "; this build supports 1");
                } else {
                    document_.format_version = 1;
                }
            } else {
                if (value.kind() != SceneValueKind::String) {
                    core_.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                                "header 'name' must be a string");
                    continue;
                }
                document_.name = value.as_string();
            }
        }

        if (seen.find("format") == seen.end()) {
            core_.error(SceneDiagnosticCode::MissingFormat, core_.peek().span,
                        "missing required header 'format = 1'");
        }
    }

    void parse_entries() {
        std::set<std::string> style_names;
        std::set<std::string> color_names;

        while (!core_.at_end()) {
            if (!core_.check(SceneTokenKind::Identifier)) {
                core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                            "expected 'style' or 'color'");
                core_.advance();
                continue;
            }

            const std::string keyword = core_.peek().text;
            if (keyword == "style") {
                parse_style(style_names);
            } else if (keyword == "color") {
                parse_color(color_names);
            } else {
                core_.error(SceneDiagnosticCode::UnexpectedToken, core_.peek().span,
                            "expected 'style' or 'color', found '" + keyword + "'");
                core_.advance();
                core_.recover_statement();
            }
        }
    }

    void parse_style(std::set<std::string>& style_names) {
        core_.advance(); // 'style'
        SceneToken name_token;
        if (!core_.expect(SceneTokenKind::String, "expected a style name string", &name_token)) {
            core_.recover_statement();
            return;
        }
        if (name_token.text.empty()) {
            core_.error(SceneDiagnosticCode::InvalidName, name_token.span,
                        "style name cannot be empty");
        }
        if (!style_names.insert(name_token.text).second) {
            core_.error(SceneDiagnosticCode::DuplicateProperty, name_token.span,
                        "duplicate style '" + name_token.text + "'");
        }
        if (!core_.expect(SceneTokenKind::LeftBrace, "expected '{' after the style name")) {
            core_.recover_statement();
            return;
        }

        ThemeStyleDesc style;
        style.name = name_token.text;
        style.span = name_token.span;

        std::set<std::string> fields;
        bool closed = false;
        while (!core_.at_end()) {
            if (core_.match(SceneTokenKind::RightBrace)) {
                closed = true;
                break;
            }

            SceneSourceSpan key_span;
            const std::string key = core_.parse_key(&key_span);
            if (key.empty()) {
                core_.recover_statement();
                continue;
            }
            if (!fields.insert(key).second) {
                core_.error(SceneDiagnosticCode::DuplicateProperty, key_span,
                            "duplicate style field '" + key + "'");
            }
            if (!core_.expect(SceneTokenKind::Equals,
                              "expected '=' after style field '" + key + "'")) {
                core_.recover_statement();
                continue;
            }

            style.fields.push_back(SceneProperty{key, core_.parse_value(), key_span});
        }

        if (!closed) {
            core_.error(SceneDiagnosticCode::UnexpectedToken, name_token.span,
                        "unterminated style block for '" + style.name + "'");
        }
        document_.styles.push_back(std::move(style));
    }

    void parse_color(std::set<std::string>& color_names) {
        core_.advance(); // 'color'
        SceneToken name_token;
        if (!core_.expect(SceneTokenKind::String, "expected a color key string", &name_token)) {
            core_.recover_statement();
            return;
        }
        if (name_token.text.empty()) {
            core_.error(SceneDiagnosticCode::InvalidName, name_token.span,
                        "color name cannot be empty");
        }
        if (!color_names.insert(name_token.text).second) {
            core_.error(SceneDiagnosticCode::DuplicateProperty, name_token.span,
                        "duplicate color '" + name_token.text + "'");
        }
        if (!core_.expect(SceneTokenKind::Equals, "expected '=' after the color key")) {
            core_.recover_statement();
            return;
        }

        ThemeColorDesc color;
        color.name = name_token.text;
        color.value = core_.parse_value();
        color.span = name_token.span;
        document_.colors.push_back(std::move(color));
    }

    ParserCore core_;
    ThemeDocument document_;
};

} // namespace

ThemeParseResult parse_theme(std::string_view text, const SceneParseOptions& options) {
    SceneParseOptions normalized = options;
    if (normalized.source_name.empty()) normalized.source_name = "<memory>";
    if (text.size() > normalized.max_bytes) {
        ThemeParseResult result;
        result.diagnostics.push_back(make_diagnostic(
            SceneDiagnosticCode::IoError, normalized.source_name, {},
            "file is larger than the configured maximum (" +
                std::to_string(normalized.max_bytes) + " bytes)"));
        return result;
    }

    detail::SceneLexResult lexed = detail::lex_scene(text, normalized.source_name);
    ThemeParser parser(std::move(lexed), std::move(normalized));
    return parser.parse();
}

ThemeParseResult parse_theme_file(const std::filesystem::path& path,
                                  const SceneParseOptions& options) {
    SceneParseOptions normalized = options;
    normalized.source_name = path.string();

    detail::TextFile file = detail::read_text_file(path, normalized.max_bytes);
    if (!file.ok) {
        ThemeParseResult result;
        result.diagnostics.push_back(make_diagnostic(
            SceneDiagnosticCode::IoError, normalized.source_name, {}, file.error));
        return result;
    }

    return parse_theme(file.text, normalized);
}

ThemeBuildResult build_theme(const ThemeDocument& document) {
    ThemeBuildResult result;
    result.theme = std::make_unique<Theme>();

    detail::SceneDiagnosticSink sink;
    sink.source = document.source;
    sink.diagnostics = &result.diagnostics;

    if (document.format_version != 1) {
        sink.error(SceneDiagnosticCode::UnsupportedVersion, {},
                   "unsupported theme format version " +
                       std::to_string(document.format_version) + "; this build supports 1");
    }

    for (const ThemeStyleDesc& style : document.styles) {
        std::vector<SceneNamedValue> members;
        members.reserve(style.fields.size());
        for (const SceneProperty& field : style.fields) {
            members.push_back(SceneNamedValue{field.name, field.value, field.span});
        }

        const SceneValue object = SceneValue::object(std::move(members), style.span);
        const std::string property_name = "style '" + style.name + "'";
        const std::optional<StyleBox> parsed =
            detail::read_style_object(object, sink, property_name);
        if (parsed) result.theme->set_style(style.name, *parsed);
    }

    for (const ThemeColorDesc& color : document.colors) {
        const std::optional<std::uint16_t> parsed =
            detail::read_color(color.value, sink, "color '" + color.name + "'");
        if (parsed) result.theme->set_color(color.name, *parsed);
    }

    return result;
}

namespace {
std::string color_value_source(const SceneValue& value) {
    if (value.kind() == SceneValueKind::String) {
        std::uint16_t color = 0;
        if (detail::parse_color_name(value.as_string(), color)) {
            return detail::color_to_source(color);
        }
    } else if (value.kind() == SceneValueKind::Int) {
        const long long color = value.as_int();
        if (color >= 0 && color <= 15) {
            return detail::color_to_source(static_cast<std::uint16_t>(color));
        }
    }
    return detail::value_to_source(value);
}
} // namespace

std::string write_theme(const ThemeDocument& document, const ThemeWriteOptions& options) {
    if (document.format_version != 1) {
        throw SceneError({make_diagnostic(
            SceneDiagnosticCode::UnsupportedVersion, document.source, {},
            "cannot write theme format version " +
                std::to_string(document.format_version) + "; this build writes 1")});
    }

    for (const ThemeStyleDesc& style : document.styles) {
        if (style.name.empty()) {
            throw SceneError({make_diagnostic(
                SceneDiagnosticCode::InvalidName, document.source, style.span,
                "cannot write a theme style with an empty name")});
        }
    }
    for (const ThemeColorDesc& color : document.colors) {
        if (color.name.empty()) {
            throw SceneError({make_diagnostic(
                SceneDiagnosticCode::InvalidName, document.source, color.span,
                "cannot write a theme color with an empty name")});
        }
    }

    const int indent_width = options.indent > 0 ? options.indent : 4;
    const std::string indent(static_cast<std::size_t>(indent_width), ' ');

    std::vector<const ThemeStyleDesc*> styles;
    styles.reserve(document.styles.size());
    for (const ThemeStyleDesc& style : document.styles) styles.push_back(&style);
    std::sort(styles.begin(), styles.end(),
              [](const ThemeStyleDesc* lhs, const ThemeStyleDesc* rhs) {
                  return lhs->name < rhs->name;
              });

    std::vector<const ThemeColorDesc*> colors;
    colors.reserve(document.colors.size());
    for (const ThemeColorDesc& color : document.colors) colors.push_back(&color);
    std::sort(colors.begin(), colors.end(),
              [](const ThemeColorDesc* lhs, const ThemeColorDesc* rhs) {
                  return lhs->name < rhs->name;
              });

    std::string out;
    out += "format = 1\n";
    if (!document.name.empty()) {
        out += "name = " + detail::string_to_source(document.name) + "\n";
    }

    if (!styles.empty() || !colors.empty()) out += "\n";

    for (std::size_t i = 0; i < styles.size(); ++i) {
        const ThemeStyleDesc& style = *styles[i];
        out += "style " + detail::string_to_source(style.name) + " {";

        std::set<std::string> written;
        std::vector<std::string> lines;
        for (const std::string& field : STYLE_FIELDS) {
            for (const SceneProperty& candidate : style.fields) {
                if (candidate.name != field) continue;
                written.insert(field);
                lines.push_back(field + " = " +
                                detail::style_field_to_source(field, candidate.value));
                break;
            }
        }

        std::vector<const SceneProperty*> unknown;
        for (const SceneProperty& candidate : style.fields) {
            if (written.find(candidate.name) == written.end()) unknown.push_back(&candidate);
        }
        std::sort(unknown.begin(), unknown.end(),
                  [](const SceneProperty* lhs, const SceneProperty* rhs) {
                      return lhs->name < rhs->name;
                  });
        for (const SceneProperty* candidate : unknown) {
            lines.push_back(candidate->name + " = " +
                            detail::style_field_to_source(candidate->name, candidate->value));
        }

        if (lines.empty()) {
            out += "}\n";
        } else {
            out += '\n';
            for (const std::string& line : lines) {
                out += indent + line + "\n";
            }
            out += "}\n";
        }

        if (i + 1 < styles.size() || !colors.empty()) out += '\n';
    }

    for (std::size_t i = 0; i < colors.size(); ++i) {
        const ThemeColorDesc& color = *colors[i];
        out += "color " + detail::string_to_source(color.name) + " = " +
               color_value_source(color.value) + "\n";
        if (i + 1 < colors.size()) out += '\n';
    }

    if (options.trailing_newline && (out.empty() || out.back() != '\n')) out += '\n';
    if (!options.trailing_newline && !out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

void write_theme_file(const ThemeDocument& document,
                      const std::filesystem::path& path,
                      const ThemeWriteOptions& options) {
    const std::string text = write_theme(document, options);
    std::string error;
    if (!detail::write_text_file(path, text, &error)) {
        throw SceneError({make_diagnostic(SceneDiagnosticCode::IoError, path.string(), {},
                                          std::move(error))});
    }
}

} // namespace guayaba
