#ifndef GUAYABA_SCENE_SCENESCHEMA_HPP
#define GUAYABA_SCENE_SCENESCHEMA_HPP

#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba {

/*
    The value shapes a built-in property accepts. The editor uses this to pick an
    inspector widget; the loader uses it to validate the AST.
*/
enum class ScenePropertyKind {
    Bool,
    Int,
    Float,
    String,
    Color,
    Glyph,
    GlyphPair,
    Vec2,
    EdgeSize,
    Range,
    Enum,
    StringArray,
    FloatArray,
    IndexColorArray,
    Style,
};

struct ScenePropertyInfo {
    std::string name;
    ScenePropertyKind kind = ScenePropertyKind::String;
    SceneValue default_value;
    std::vector<std::string> enum_values;
    bool nullable = false;
    bool single_line = false;
    bool non_negative = false;
    std::string category;
};

struct SceneTypeInfo {
    std::string name;
    std::string base;
    // Flattened: inherited properties first, then own properties, in application
    // and canonical-write order. Hidden inherited properties are not present.
    std::vector<ScenePropertyInfo> properties;
};

/*
    Read-only description of the built-in scene types. The loader keeps the
    setters and factories private; this view is what an editor needs to build a
    node palette and a property inspector.

    There is deliberately no registration API in this version. The registry
    behind the schema is already separated from the format, so exposing custom
    types later does not change the file grammar.
*/
class SceneSchema {
public:
    static const SceneSchema& builtins();

    const SceneTypeInfo* find_type(std::string_view name) const;
    const ScenePropertyInfo* find_property(std::string_view type,
                                           std::string_view property) const;

    const std::vector<SceneTypeInfo>& types() const { return types_; }

private:
    SceneSchema();

    std::vector<SceneTypeInfo> types_;
};

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENESCHEMA_HPP
