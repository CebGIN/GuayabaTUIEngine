#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "SceneText.hpp"

#include "SceneFormatting.hpp"

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

std::string color_value_to_source(const SceneValue& value) {
    if (value.kind() == SceneValueKind::String) {
        std::uint16_t color = 0;
        if (parse_color_name(value.as_string(), color)) return color_to_source(color);
    } else if (value.kind() == SceneValueKind::Int) {
        const long long color = value.as_int();
        if (color >= 0 && color <= 15) return color_to_source(static_cast<std::uint16_t>(color));
    }
    return value_to_source(value);
}

const SceneNamedValue* find_member(const SceneValue& object, std::string_view name) {
    for (const SceneNamedValue& member : object.members()) {
        if (member.name == name) return &member;
    }
    return nullptr;
}

} // namespace

std::string string_to_source(std::string_view text) {
    return "\"" + escape_string(text) + "\"";
}

std::string color_to_source(std::uint16_t color) {
    return string_to_source(canonical_color_name(color));
}

std::string value_to_source(const SceneValue& value) {
    switch (value.kind()) {
        case SceneValueKind::Null:
            return "none";
        case SceneValueKind::Bool:
            return value.as_bool() ? "true" : "false";
        case SceneValueKind::Int:
            return std::to_string(value.as_int());
        case SceneValueKind::Float:
            return format_float(value.as_float());
        case SceneValueKind::String:
            return string_to_source(value.as_string());
        case SceneValueKind::Array: {
            std::string out = "[";
            for (std::size_t i = 0; i < value.items().size(); ++i) {
                if (i != 0) out += ", ";
                out += value_to_source(value.items()[i]);
            }
            out += ']';
            return out;
        }
        case SceneValueKind::Object: {
            std::vector<const SceneNamedValue*> members;
            members.reserve(value.members().size());
            for (const SceneNamedValue& member : value.members()) {
                members.push_back(&member);
            }
            std::sort(members.begin(), members.end(),
                      [](const SceneNamedValue* lhs, const SceneNamedValue* rhs) {
                          return lhs->name < rhs->name;
                      });

            std::string out = "{ ";
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (i != 0) out += ", ";
                out += members[i]->name;
                out += " = ";
                out += value_to_source(members[i]->value);
            }
            if (!members.empty()) out += ' ';
            out += '}';
            return out;
        }
    }
    return "none";
}

std::string style_to_source(const SceneValue& value) {
    if (value.kind() != SceneValueKind::Object) return value_to_source(value);

    static const std::vector<std::string> fields{
        "background", "border", "border_style", "border_width", "corner_radius", "margin",
    };

    std::vector<std::string> parts;
    std::set<std::string> known;

    for (const std::string& field : fields) {
        known.insert(field);
        const SceneNamedValue* member = find_member(value, field);
        if (member == nullptr) continue;
        if ((field == "background" || field == "border") &&
            member->value.kind() == SceneValueKind::Null) {
            continue;
        }

        std::string rendered;
        if (field == "background" || field == "border") {
            rendered = color_value_to_source(member->value);
        } else if (field == "border_style") {
            rendered = enum_to_source(member->value,
                                      {"none", "ascii", "double", "rounded", "heavy", "dashed"});
        } else {
            rendered = value_to_source(member->value);
        }
        parts.push_back(field + " = " + rendered);
    }

    std::vector<const SceneNamedValue*> unknown;
    for (const SceneNamedValue& member : value.members()) {
        if (known.find(member.name) == known.end()) unknown.push_back(&member);
    }
    std::sort(unknown.begin(), unknown.end(),
              [](const SceneNamedValue* lhs, const SceneNamedValue* rhs) {
                  return lhs->name < rhs->name;
              });
    for (const SceneNamedValue* member : unknown) {
        parts.push_back(member->name + " = " + value_to_source(member->value));
    }

    if (parts.empty()) return "none";

    std::string out = "{ ";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += ", ";
        out += parts[i];
    }
    out += " }";
    return out;
}

std::string style_field_to_source(std::string_view field, const SceneValue& value) {
    if (field == "background" || field == "border") {
        if (value.kind() == SceneValueKind::Null) return "none";
        return color_value_to_source(value);
    }
    if (field == "border_style") {
        return enum_to_source(value, {"none", "ascii", "double", "rounded", "heavy", "dashed"});
    }
    if (field == "margin" || field == "border_width" || field == "corner_radius") {
        return value_to_source(value);
    }
    return value_to_source(value);
}

std::string enum_to_source(const SceneValue& value,
                           const std::vector<std::string>& choices) {
    if (value.kind() != SceneValueKind::String) return value_to_source(value);

    const std::string wanted = lower_ascii(value.as_string());
    for (const std::string& choice : choices) {
        if (wanted == choice) return string_to_source(choice);
    }
    return value_to_source(value);
}

} // namespace guayaba::detail
