#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "SceneText.hpp"

#include "SceneValueUtils.hpp"

namespace guayaba::detail {

namespace {

std::string lower_ascii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

std::string choices_text(const std::vector<std::string>& choices) {
    std::string out;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (i != 0) out += ", ";
        out += choices[i];
    }
    return out;
}

bool is_finite_number(const SceneValue& value, double& out) {
    if (value.kind() == SceneValueKind::Int) {
        out = static_cast<double>(value.as_int());
        return std::isfinite(out);
    }
    if (value.kind() == SceneValueKind::Float) {
        out = value.as_float();
        return std::isfinite(out);
    }
    return false;
}

} // namespace

void SceneDiagnosticSink::error(SceneDiagnosticCode code,
                                SceneSourceSpan span,
                                std::string message) {
    if (!node_path.empty()) message = "at " + node_path + ": " + message;
    if (diagnostics) {
        diagnostics->push_back(make_diagnostic(code, source, span, std::move(message)));
    }
}

void SceneDiagnosticSink::warning(SceneDiagnosticCode code,
                                  SceneSourceSpan span,
                                  std::string message) {
    if (!node_path.empty()) message = "at " + node_path + ": " + message;
    if (diagnostics) {
        diagnostics->push_back(make_diagnostic(
            code, source, span, std::move(message), SceneDiagnosticSeverity::Warning));
    }
}

std::optional<bool> read_bool(const SceneValue& value,
                              SceneDiagnosticSink& sink,
                              const std::string& property) {
    if (value.kind() != SceneValueKind::Bool) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects true or false");
        return std::nullopt;
    }
    return value.as_bool();
}

std::optional<long long> read_int(const SceneValue& value,
                                  SceneDiagnosticSink& sink,
                                  const std::string& property,
                                  long long minimum,
                                  long long maximum) {
    if (value.kind() != SceneValueKind::Int) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects an integer");
        return std::nullopt;
    }

    const long long number = value.as_int();
    if (number < minimum || number > maximum) {
        sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                   "property '" + property + "' is outside its allowed range");
        return std::nullopt;
    }
    return number;
}

std::optional<double> read_float(const SceneValue& value,
                                 SceneDiagnosticSink& sink,
                                 const std::string& property,
                                 double minimum,
                                 double maximum) {
    double number = 0.0;
    if (!is_finite_number(value, number)) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects a finite number");
        return std::nullopt;
    }
    if (number < minimum || number > maximum) {
        sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                   "property '" + property + "' is outside its allowed range");
        return std::nullopt;
    }
    return number;
}

std::optional<std::string> read_string(const SceneValue& value,
                                       SceneDiagnosticSink& sink,
                                       const std::string& property,
                                       bool single_line) {
    if (value.kind() != SceneValueKind::String) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects a string");
        return std::nullopt;
    }

    const std::string& text = value.as_string();
    if (!is_valid_utf8(text)) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' is not valid UTF-8");
        return std::nullopt;
    }
    if (single_line && has_line_break(text)) {
        sink.error(SceneDiagnosticCode::ValueOutOfRange, value.span(),
                   "property '" + property + "' cannot contain line breaks");
        return std::nullopt;
    }
    return text;
}

std::optional<std::vector<std::string>> read_string_array(const SceneValue& value,
                                                          SceneDiagnosticSink& sink,
                                                          const std::string& property,
                                                          bool single_line) {
    if (value.kind() != SceneValueKind::Array) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects an array of strings");
        return std::nullopt;
    }

    std::vector<std::string> out;
    out.reserve(value.items().size());
    for (const SceneValue& item : value.items()) {
        if (item.kind() != SceneValueKind::String) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' expects only strings");
            return std::nullopt;
        }
        const std::string& text = item.as_string();
        if (!is_valid_utf8(text)) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' contains invalid UTF-8");
            return std::nullopt;
        }
        if (single_line && has_line_break(text)) {
            sink.error(SceneDiagnosticCode::ValueOutOfRange, item.span(),
                       "property '" + property + "' cannot contain line breaks");
            return std::nullopt;
        }
        out.push_back(text);
    }
    return out;
}

std::optional<std::vector<double>> read_float_array(const SceneValue& value,
                                                    SceneDiagnosticSink& sink,
                                                    const std::string& property) {
    if (value.kind() != SceneValueKind::Array) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects an array of numbers");
        return std::nullopt;
    }

    std::vector<double> out;
    out.reserve(value.items().size());
    for (const SceneValue& item : value.items()) {
        double number = 0.0;
        if (!is_finite_number(item, number)) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' expects finite numbers");
            return std::nullopt;
        }
        out.push_back(number);
    }
    return out;
}

std::optional<Vec2> read_vec2(const SceneValue& value,
                              SceneDiagnosticSink& sink,
                              const std::string& property,
                              bool non_negative) {
    if (value.kind() != SceneValueKind::Array || value.items().size() != 2) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects [x, y]");
        return std::nullopt;
    }

    int components[2] = {0, 0};
    for (std::size_t i = 0; i < 2; ++i) {
        const SceneValue& item = value.items()[i];
        if (item.kind() != SceneValueKind::Int) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' expects integer components");
            return std::nullopt;
        }
        const long long number = item.as_int();
        if (number < std::numeric_limits<int>::min() ||
            number > std::numeric_limits<int>::max()) {
            sink.error(SceneDiagnosticCode::ValueOutOfRange, item.span(),
                       "property '" + property + "' component is out of range");
            return std::nullopt;
        }
        if (non_negative && number < 0) {
            sink.error(SceneDiagnosticCode::ValueOutOfRange, item.span(),
                       "property '" + property + "' cannot have negative components");
            return std::nullopt;
        }
        components[i] = static_cast<int>(number);
    }
    return Vec2{components[0], components[1]};
}

std::optional<EdgeSize> read_edge_size(const SceneValue& value,
                                       SceneDiagnosticSink& sink,
                                       const std::string& property) {
    if (value.kind() != SceneValueKind::Array || value.items().size() != 4) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects [left, top, right, bottom]");
        return std::nullopt;
    }

    int components[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < 4; ++i) {
        const SceneValue& item = value.items()[i];
        if (item.kind() != SceneValueKind::Int) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' expects integer components");
            return std::nullopt;
        }
        const long long number = item.as_int();
        if (number < 0 || number > std::numeric_limits<int>::max()) {
            sink.error(SceneDiagnosticCode::ValueOutOfRange, item.span(),
                       "property '" + property + "' components must be non-negative");
            return std::nullopt;
        }
        components[i] = static_cast<int>(number);
    }
    return EdgeSize{components[0], components[1], components[2], components[3]};
}

std::optional<std::pair<double, double>> read_range(const SceneValue& value,
                                                    SceneDiagnosticSink& sink,
                                                    const std::string& property) {
    if (value.kind() != SceneValueKind::Array || value.items().size() != 2) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects [min, max]");
        return std::nullopt;
    }

    double numbers[2] = {0.0, 0.0};
    for (std::size_t i = 0; i < 2; ++i) {
        if (!is_finite_number(value.items()[i], numbers[i])) {
            sink.error(SceneDiagnosticCode::TypeMismatch, value.items()[i].span(),
                       "property '" + property + "' expects finite numbers");
            return std::nullopt;
        }
    }
    return std::make_pair(numbers[0], numbers[1]);
}

std::optional<std::uint16_t> read_color(const SceneValue& value,
                                        SceneDiagnosticSink& sink,
                                        const std::string& property) {
    if (value.kind() == SceneValueKind::String) {
        std::uint16_t color = 0;
        if (parse_color_name(value.as_string(), color)) return color;
        sink.error(SceneDiagnosticCode::InvalidColor, value.span(),
                   "property '" + property + "' has an unknown color name '" +
                       value.as_string() + "'");
        return std::nullopt;
    }

    if (value.kind() == SceneValueKind::Int) {
        const long long color = value.as_int();
        if (color >= 0 && color <= 15) return static_cast<std::uint16_t>(color);
        sink.error(SceneDiagnosticCode::InvalidColor, value.span(),
                   "property '" + property + "' expects a color index from 0 to 15");
        return std::nullopt;
    }

    sink.error(SceneDiagnosticCode::InvalidColor, value.span(),
               "property '" + property + "' expects a color name or index");
    return std::nullopt;
}

std::optional<char32_t> read_glyph(const SceneValue& value,
                                   SceneDiagnosticSink& sink,
                                   const std::string& property) {
    if (value.kind() != SceneValueKind::String) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects a one-character string");
        return std::nullopt;
    }

    char32_t codepoint = 0;
    if (!is_single_printable_glyph(value.as_string(), &codepoint)) {
        sink.error(SceneDiagnosticCode::InvalidGlyph, value.span(),
                   "property '" + property + "' expects one printable character");
        return std::nullopt;
    }
    return codepoint;
}

std::optional<std::pair<char32_t, char32_t>> read_glyph_pair(
    const SceneValue& value,
    SceneDiagnosticSink& sink,
    const std::string& property) {
    if (value.kind() != SceneValueKind::Array || value.items().size() != 2) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects [checked_glyph, unchecked_glyph]");
        return std::nullopt;
    }

    const std::optional<char32_t> checked = read_glyph(value.items()[0], sink, property);
    if (!checked) return std::nullopt;
    const std::optional<char32_t> unchecked = read_glyph(value.items()[1], sink, property);
    if (!unchecked) return std::nullopt;
    return std::make_pair(*checked, *unchecked);
}

std::optional<std::string> read_enum(const SceneValue& value,
                                     SceneDiagnosticSink& sink,
                                     const std::string& property,
                                     const std::vector<std::string>& choices) {
    if (value.kind() != SceneValueKind::String) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects one of: " + choices_text(choices));
        return std::nullopt;
    }

    const std::string wanted = lower_ascii(value.as_string());
    for (const std::string& choice : choices) {
        if (wanted == choice) return choice;
    }

    sink.error(SceneDiagnosticCode::InvalidEnum, value.span(),
               "property '" + property + "' has unknown value '" + value.as_string() +
                   "'; expected one of: " + choices_text(choices));
    return std::nullopt;
}

std::optional<std::vector<std::pair<int, std::uint16_t>>>
read_index_color_array(const SceneValue& value,
                       SceneDiagnosticSink& sink,
                       const std::string& property) {
    if (value.kind() != SceneValueKind::Array) {
        sink.error(SceneDiagnosticCode::TypeMismatch, value.span(),
                   "property '" + property + "' expects [[index, color], ...]");
        return std::nullopt;
    }

    std::vector<std::pair<int, std::uint16_t>> out;
    std::set<int> seen;
    for (const SceneValue& item : value.items()) {
        if (item.kind() != SceneValueKind::Array || item.items().size() != 2) {
            sink.error(SceneDiagnosticCode::TypeMismatch, item.span(),
                       "property '" + property + "' expects [index, color] pairs");
            return std::nullopt;
        }

        const SceneValue& index_value = item.items()[0];
        if (index_value.kind() != SceneValueKind::Int) {
            sink.error(SceneDiagnosticCode::TypeMismatch, index_value.span(),
                       "property '" + property + "' expects an integer index");
            return std::nullopt;
        }
        const long long index = index_value.as_int();
        if (index < 0 || index > std::numeric_limits<int>::max()) {
            sink.error(SceneDiagnosticCode::ValueOutOfRange, index_value.span(),
                       "property '" + property + "' index must be non-negative");
            return std::nullopt;
        }
        if (!seen.insert(static_cast<int>(index)).second) {
            sink.error(SceneDiagnosticCode::DuplicateProperty, index_value.span(),
                       "property '" + property + "' repeats index " +
                           std::to_string(index));
            return std::nullopt;
        }

        const std::optional<std::uint16_t> color =
            read_color(item.items()[1], sink, property);
        if (!color) return std::nullopt;
        out.emplace_back(static_cast<int>(index), *color);
    }
    return out;
}

std::optional<StyleBox> read_style_object(const SceneValue& value,
                                          SceneDiagnosticSink& sink,
                                          const std::string& property) {
    if (value.kind() != SceneValueKind::Object) {
        sink.error(SceneDiagnosticCode::InvalidStyle, value.span(),
                   "property '" + property + "' expects a style table");
        return std::nullopt;
    }

    StyleBox style;
    std::set<std::string> seen;

    for (const SceneNamedValue& member : value.members()) {
        if (!seen.insert(member.name).second) {
            sink.error(SceneDiagnosticCode::DuplicateProperty, member.name_span,
                       "style '" + property + "' repeats field '" + member.name + "'");
            return std::nullopt;
        }

        const std::string field = property + "." + member.name;
        if (member.name == "background" || member.name == "border") {
            if (member.value.kind() == SceneValueKind::Null) {
                if (member.name == "background") style.clear_background_color();
                else style.clear_border_color();
                continue;
            }
            const std::optional<std::uint16_t> color = read_color(member.value, sink, field);
            if (!color) return std::nullopt;
            if (member.name == "background") style.set_background_color(*color);
            else style.set_border_color(*color);
            continue;
        }

        if (member.name == "border_style") {
            const std::optional<std::string> style_name = read_enum(
                member.value, sink, field,
                {"none", "ascii", "double", "rounded", "heavy", "dashed"});
            if (!style_name) return std::nullopt;
            if (*style_name == "none") style.set_border_style(StyleBorder::NONE);
            else if (*style_name == "ascii") style.set_border_style(StyleBorder::ASCII);
            else if (*style_name == "double") style.set_border_style(StyleBorder::DOUBLE);
            else if (*style_name == "rounded") style.set_border_style(StyleBorder::ROUNDED);
            else if (*style_name == "heavy") style.set_border_style(StyleBorder::HEAVY);
            else style.set_border_style(StyleBorder::DASHED);
            continue;
        }

        if (member.name == "border_width") {
            const std::optional<long long> width =
                read_int(member.value, sink, field, 0, std::numeric_limits<int>::max());
            if (!width) return std::nullopt;
            style.set_border_width(static_cast<int>(*width));
            continue;
        }

        if (member.name == "corner_radius") {
            const std::optional<long long> radius =
                read_int(member.value, sink, field, 0, std::numeric_limits<int>::max());
            if (!radius) return std::nullopt;
            style.set_corner_radius(static_cast<int>(*radius));
            continue;
        }

        if (member.name == "margin") {
            const std::optional<EdgeSize> margin = read_edge_size(member.value, sink, field);
            if (!margin) return std::nullopt;
            style.set_content_margin(*margin);
            continue;
        }

        sink.error(SceneDiagnosticCode::InvalidStyle, member.name_span,
                   "unknown style field '" + member.name + "'");
        return std::nullopt;
    }

    return style;
}

std::optional<StyleBox> read_style(const SceneValue& value,
                                   SceneDiagnosticSink& sink,
                                   const Theme* theme,
                                   const std::string& property) {
    if (value.kind() == SceneValueKind::Null) return StyleBox{};

    if (value.kind() == SceneValueKind::String) {
        const std::string& key = value.as_string();
        if (!theme) {
            sink.error(SceneDiagnosticCode::InvalidStyle, value.span(),
                       "property '" + property + "' references style '" + key +
                           "' but no theme is loaded");
            return std::nullopt;
        }
        if (!theme->has_style(key)) {
            sink.error(SceneDiagnosticCode::InvalidStyle, value.span(),
                       "theme has no style named '" + key + "'");
            return std::nullopt;
        }
        const StyleBox empty;
        return theme->get_style(key, empty);
    }

    return read_style_object(value, sink, property);
}

} // namespace guayaba::detail
