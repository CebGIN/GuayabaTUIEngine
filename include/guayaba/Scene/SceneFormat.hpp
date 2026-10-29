#ifndef GUAYABA_SCENE_SCENEFORMAT_HPP
#define GUAYABA_SCENE_SCENEFORMAT_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace guayaba {

// --- Source locations -------------------------------------------------------

struct SceneSourceLocation {
    std::size_t offset = 0;   // Byte offset into the source.
    std::uint32_t line = 1;   // 1-based, counted in codepoints.
    std::uint32_t column = 1; // 1-based, counted in codepoints.
};

struct SceneSourceSpan {
    SceneSourceLocation begin;
    std::size_t length = 0; // Bytes.

    std::size_t end_offset() const { return begin.offset + length; }
};

// --- Diagnostics ------------------------------------------------------------

enum class SceneDiagnosticSeverity {
    Error,
    Warning,
};

enum class SceneDiagnosticCode {
    IoError,
    UnexpectedToken,
    InvalidUtf8,
    UnterminatedString,
    InvalidEscape,
    InvalidNumber,
    DuplicateHeaderKey,
    DuplicateProperty,
    DuplicateSibling,
    MissingFormat,
    MissingRoot,
    MultipleRoots,
    UnsupportedVersion,
    InvalidName,
    UnknownType,
    UnknownProperty,
    TypeMismatch,
    ValueOutOfRange,
    InvalidColor,
    InvalidEnum,
    InvalidStyle,
    InvalidGlyph,
    ThemeNotFound,
    PropertyRejected,
    InvalidDocument,
};

struct SceneDiagnostic {
    SceneDiagnosticCode code = SceneDiagnosticCode::InvalidDocument;
    SceneDiagnosticSeverity severity = SceneDiagnosticSeverity::Error;
    std::string source;
    SceneSourceSpan span;
    std::string message;

    // "source:line:column: message"
    std::string format() const;
};

class SceneError : public std::runtime_error {
public:
    explicit SceneError(std::vector<SceneDiagnostic> diagnostics);

    const std::vector<SceneDiagnostic>& diagnostics() const { return diagnostics_; }

private:
    static std::string build_message(const std::vector<SceneDiagnostic>& diagnostics);

    std::vector<SceneDiagnostic> diagnostics_;
};

// --- Values -----------------------------------------------------------------

enum class SceneValueKind {
    Null,
    Bool,
    Int,
    Float,
    String,
    Array,
    Object,
};

struct SceneNamedValue;

/*
    A format value. The AST keeps the source span of every value so the editor can
    map a selection or a diagnostic back to the file without a second parse.

    Object values keep their member order: the canonical writer uses a fixed order
    for known fields and alphabetical order for unknown ones, but the parser must
    not lose what the author wrote before the editor rewrites it.
*/
class SceneValue {
public:
    SceneValue();
    SceneValue(const SceneValue& other);
    SceneValue(SceneValue&& other) noexcept;
    SceneValue& operator=(const SceneValue& other);
    SceneValue& operator=(SceneValue&& other) noexcept;
    ~SceneValue();

    static SceneValue null(SceneSourceSpan span = {});
    static SceneValue boolean(bool value, SceneSourceSpan span = {});
    static SceneValue integer(long long value, SceneSourceSpan span = {});
    static SceneValue number(double value, SceneSourceSpan span = {});
    static SceneValue string(std::string value, SceneSourceSpan span = {});
    static SceneValue array(std::vector<SceneValue> values, SceneSourceSpan span = {});
    static SceneValue object(std::vector<SceneNamedValue> members, SceneSourceSpan span = {});

    SceneValueKind kind() const { return kind_; }
    SceneSourceSpan span() const { return span_; }
    void set_span(SceneSourceSpan span) { span_ = span; }

    bool as_bool() const;
    long long as_int() const;
    double as_float() const;
    const std::string& as_string() const;

    const std::vector<SceneValue>& items() const { return items_; }
    std::vector<SceneValue>& items() { return items_; }

    const std::vector<SceneNamedValue>& members() const { return members_; }
    std::vector<SceneNamedValue>& members() { return members_; }

private:
    SceneValueKind kind_ = SceneValueKind::Null;
    bool bool_value_ = false;
    long long int_value_ = 0;
    double float_value_ = 0.0;
    std::string string_value_;
    std::vector<SceneValue> items_;
    std::vector<SceneNamedValue> members_;
    SceneSourceSpan span_;
};

struct SceneNamedValue {
    std::string name;
    SceneValue value;
    SceneSourceSpan name_span;
};

bool operator==(const SceneValue& lhs, const SceneValue& rhs);
bool operator!=(const SceneValue& lhs, const SceneValue& rhs);

// --- Document ---------------------------------------------------------------

struct SceneProperty {
    std::string name;
    SceneValue value;
    SceneSourceSpan span;
};

struct SceneNodeDesc {
    std::string type;
    std::string name;
    SceneSourceSpan span;
    std::vector<SceneProperty> properties;
    std::vector<std::unique_ptr<SceneNodeDesc>> children;
};

struct SceneDocument {
    int format_version = 1;
    std::string name;
    std::string theme;
    std::unique_ptr<SceneNodeDesc> root;

    SceneDocument() = default;
    SceneDocument(const SceneDocument&) = delete;
    SceneDocument& operator=(const SceneDocument&) = delete;
    SceneDocument(SceneDocument&&) = default;
    SceneDocument& operator=(SceneDocument&&) = default;
};

// --- Helpers ----------------------------------------------------------------

SceneDiagnostic make_diagnostic(SceneDiagnosticCode code,
                                std::string source,
                                SceneSourceSpan span,
                                std::string message,
                                SceneDiagnosticSeverity severity = SceneDiagnosticSeverity::Error);

bool has_error(const std::vector<SceneDiagnostic>& diagnostics);

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEFORMAT_HPP
