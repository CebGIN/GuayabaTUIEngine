#ifndef GUAYABA_SCENE_SCENELEXER_HPP
#define GUAYABA_SCENE_SCENELEXER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba::detail {

enum class SceneTokenKind {
    End,
    Identifier,
    String,
    Integer,
    Float,
    True,
    False,
    None,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Equals,
    Comma,
    Dot,
};

struct SceneToken {
    SceneTokenKind kind = SceneTokenKind::End;
    std::string text;
    long long int_value = 0;
    double float_value = 0.0;
    SceneSourceSpan span;
};

struct SceneLexResult {
    std::vector<SceneToken> tokens;
    std::vector<SceneDiagnostic> diagnostics;
};

SceneLexResult lex_scene(std::string_view text, const std::string& source);

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENELEXER_HPP
