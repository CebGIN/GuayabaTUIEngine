#include <sstream>
#include <utility>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba {

// --- SceneValue -------------------------------------------------------------

// The out-of-line special members let SceneValue hold a vector of
// SceneNamedValue, which in turn holds a SceneValue. Defining them here, after
// SceneNamedValue is complete, keeps the public header free of a recursive
// incomplete-type problem.
SceneValue::SceneValue() = default;
SceneValue::SceneValue(const SceneValue& other) = default;
SceneValue::SceneValue(SceneValue&& other) noexcept = default;
SceneValue& SceneValue::operator=(const SceneValue& other) = default;
SceneValue& SceneValue::operator=(SceneValue&& other) noexcept = default;
SceneValue::~SceneValue() = default;

SceneValue SceneValue::null(SceneSourceSpan span) {
    SceneValue value;
    value.span_ = span;
    return value;
}

SceneValue SceneValue::boolean(bool boolean_value, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::Bool;
    value.bool_value_ = boolean_value;
    value.span_ = span;
    return value;
}

SceneValue SceneValue::integer(long long int_value, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::Int;
    value.int_value_ = int_value;
    value.span_ = span;
    return value;
}

SceneValue SceneValue::number(double float_value, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::Float;
    value.float_value_ = float_value;
    value.span_ = span;
    return value;
}

SceneValue SceneValue::string(std::string string_value, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::String;
    value.string_value_ = std::move(string_value);
    value.span_ = span;
    return value;
}

SceneValue SceneValue::array(std::vector<SceneValue> values, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::Array;
    value.items_ = std::move(values);
    value.span_ = span;
    return value;
}

SceneValue SceneValue::object(std::vector<SceneNamedValue> members, SceneSourceSpan span) {
    SceneValue value;
    value.kind_ = SceneValueKind::Object;
    value.members_ = std::move(members);
    value.span_ = span;
    return value;
}

bool SceneValue::as_bool() const {
    if (kind_ != SceneValueKind::Bool) {
        throw std::logic_error("SceneValue: value is not a bool.");
    }
    return bool_value_;
}

long long SceneValue::as_int() const {
    if (kind_ != SceneValueKind::Int) {
        throw std::logic_error("SceneValue: value is not an int.");
    }
    return int_value_;
}

double SceneValue::as_float() const {
    if (kind_ != SceneValueKind::Float) {
        throw std::logic_error("SceneValue: value is not a float.");
    }
    return float_value_;
}

const std::string& SceneValue::as_string() const {
    if (kind_ != SceneValueKind::String) {
        throw std::logic_error("SceneValue: value is not a string.");
    }
    return string_value_;
}

bool operator==(const SceneValue& lhs, const SceneValue& rhs) {
    if (lhs.kind() != rhs.kind()) return false;

    switch (lhs.kind()) {
        case SceneValueKind::Null:
            return true;
        case SceneValueKind::Bool:
            return lhs.as_bool() == rhs.as_bool();
        case SceneValueKind::Int:
            return lhs.as_int() == rhs.as_int();
        case SceneValueKind::Float:
            return lhs.as_float() == rhs.as_float();
        case SceneValueKind::String:
            return lhs.as_string() == rhs.as_string();
        case SceneValueKind::Array:
            return lhs.items() == rhs.items();
        case SceneValueKind::Object:
            if (lhs.members().size() != rhs.members().size()) return false;
            for (std::size_t i = 0; i < lhs.members().size(); ++i) {
                if (lhs.members()[i].name != rhs.members()[i].name) return false;
                if (lhs.members()[i].value != rhs.members()[i].value) return false;
            }
            return true;
    }
    return false;
}

bool operator!=(const SceneValue& lhs, const SceneValue& rhs) {
    return !(lhs == rhs);
}

// --- Diagnostics ------------------------------------------------------------

std::string SceneDiagnostic::format() const {
    std::ostringstream out;
    out << (source.empty() ? "<memory>" : source)
        << ':' << span.begin.line << ':' << span.begin.column
        << ": " << message;
    return out.str();
}

SceneError::SceneError(std::vector<SceneDiagnostic> diagnostics)
    : std::runtime_error(build_message(diagnostics)),
      diagnostics_(std::move(diagnostics)) {}

std::string SceneError::build_message(const std::vector<SceneDiagnostic>& diagnostics) {
    if (diagnostics.empty()) return "scene error";

    std::string message = diagnostics.front().format();
    std::size_t errors = 0;
    for (const SceneDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == SceneDiagnosticSeverity::Error) ++errors;
    }
    if (errors > 1) {
        message += " (and " + std::to_string(errors - 1) + " more error";
        if (errors > 2) message += 's';
        message += ')';
    }
    return message;
}

SceneDiagnostic make_diagnostic(SceneDiagnosticCode code,
                                std::string source,
                                SceneSourceSpan span,
                                std::string message,
                                SceneDiagnosticSeverity severity) {
    SceneDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.source = std::move(source);
    diagnostic.span = span;
    diagnostic.message = std::move(message);
    return diagnostic;
}

bool has_error(const std::vector<SceneDiagnostic>& diagnostics) {
    for (const SceneDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == SceneDiagnosticSeverity::Error) return true;
    }
    return false;
}

} // namespace guayaba
