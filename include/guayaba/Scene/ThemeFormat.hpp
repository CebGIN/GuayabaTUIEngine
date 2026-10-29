#ifndef GUAYABA_SCENE_THEMEFORMAT_HPP
#define GUAYABA_SCENE_THEMEFORMAT_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Resources/Theme.hpp>
#include <guayaba/Scene/SceneFormat.hpp>
#include <guayaba/Scene/SceneParser.hpp>

namespace guayaba {

/*
    A parsed .gtheme file. The theme format is intentionally small: named StyleBoxes
    and named colours, both consumed by the built-in apply_theme() implementations.
    build_theme() validates fields and returns a Theme; write_theme() emits the
    canonical form.
*/
struct ThemeStyleDesc {
    std::string name;
    SceneSourceSpan span;
    std::vector<SceneProperty> fields;
};

struct ThemeColorDesc {
    std::string name;
    SceneValue value;
    SceneSourceSpan span;
};

struct ThemeDocument {
    int format_version = 1;
    std::string name;
    std::string source = "<memory>";
    std::vector<ThemeStyleDesc> styles;
    std::vector<ThemeColorDesc> colors;
};

struct ThemeParseResult {
    ThemeDocument document;
    std::vector<SceneDiagnostic> diagnostics;

    bool ok() const { return !has_error(diagnostics); }
};

struct ThemeBuildResult {
    std::unique_ptr<Theme> theme;
    std::vector<SceneDiagnostic> diagnostics;

    bool ok() const { return theme != nullptr && !has_error(diagnostics); }
};

struct ThemeWriteOptions {
    int indent = 4;
    bool trailing_newline = true;
};

ThemeParseResult parse_theme(std::string_view text,
                             const SceneParseOptions& options = {});

ThemeParseResult parse_theme_file(const std::filesystem::path& path,
                                  const SceneParseOptions& options = {});

ThemeBuildResult build_theme(const ThemeDocument& document);

std::string write_theme(const ThemeDocument& document,
                        const ThemeWriteOptions& options = {});

void write_theme_file(const ThemeDocument& document,
                      const std::filesystem::path& path,
                      const ThemeWriteOptions& options = {});

} // namespace guayaba

#endif // GUAYABA_SCENE_THEMEFORMAT_HPP
