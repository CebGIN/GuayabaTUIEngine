#ifndef GUAYABA_SCENE_SCENEVALUEUTILS_HPP
#define GUAYABA_SCENE_SCENEVALUEUTILS_HPP

#include <climits>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Resources/Stylebox.hpp>
#include <guayaba/Scene/SceneFormat.hpp>

#include "SceneInternal.hpp"

namespace guayaba::detail {

std::optional<bool> read_bool(const SceneValue& value,
                              SceneDiagnosticSink& sink,
                              const std::string& property);

std::optional<long long> read_int(const SceneValue& value,
                                  SceneDiagnosticSink& sink,
                                  const std::string& property,
                                  long long minimum = LLONG_MIN,
                                  long long maximum = LLONG_MAX);

std::optional<double> read_float(const SceneValue& value,
                                 SceneDiagnosticSink& sink,
                                 const std::string& property,
                                 double minimum = -1.0e300,
                                 double maximum = 1.0e300);

std::optional<std::string> read_string(const SceneValue& value,
                                       SceneDiagnosticSink& sink,
                                       const std::string& property,
                                       bool single_line = false);

std::optional<std::vector<std::string>> read_string_array(const SceneValue& value,
                                                          SceneDiagnosticSink& sink,
                                                          const std::string& property,
                                                          bool single_line = false);

std::optional<std::vector<double>> read_float_array(const SceneValue& value,
                                                    SceneDiagnosticSink& sink,
                                                    const std::string& property);

std::optional<Vec2> read_vec2(const SceneValue& value,
                              SceneDiagnosticSink& sink,
                              const std::string& property,
                              bool non_negative = false);

std::optional<EdgeSize> read_edge_size(const SceneValue& value,
                                       SceneDiagnosticSink& sink,
                                       const std::string& property);

std::optional<std::pair<double, double>> read_range(const SceneValue& value,
                                                    SceneDiagnosticSink& sink,
                                                    const std::string& property);

std::optional<std::uint16_t> read_color(const SceneValue& value,
                                        SceneDiagnosticSink& sink,
                                        const std::string& property);

std::optional<char32_t> read_glyph(const SceneValue& value,
                                   SceneDiagnosticSink& sink,
                                   const std::string& property);

std::optional<std::pair<char32_t, char32_t>> read_glyph_pair(
    const SceneValue& value,
    SceneDiagnosticSink& sink,
    const std::string& property);

std::optional<std::string> read_enum(const SceneValue& value,
                                     SceneDiagnosticSink& sink,
                                     const std::string& property,
                                     const std::vector<std::string>& choices);

std::optional<std::vector<std::pair<int, std::uint16_t>>>
read_index_color_array(const SceneValue& value,
                       SceneDiagnosticSink& sink,
                       const std::string& property);

std::optional<StyleBox> read_style(const SceneValue& value,
                                   SceneDiagnosticSink& sink,
                                   const Theme* theme,
                                   const std::string& property);

std::optional<StyleBox> read_style_object(const SceneValue& value,
                                          SceneDiagnosticSink& sink,
                                          const std::string& property);

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENEVALUEUTILS_HPP
