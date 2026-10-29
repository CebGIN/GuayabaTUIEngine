#ifndef GUAYABA_SCENE_SCENEFORMATTING_HPP
#define GUAYABA_SCENE_SCENEFORMATTING_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba::detail {

std::string string_to_source(std::string_view text);
std::string color_to_source(std::uint16_t color);
std::string value_to_source(const SceneValue& value);
std::string style_to_source(const SceneValue& value);
std::string style_field_to_source(std::string_view field, const SceneValue& value);
std::string enum_to_source(const SceneValue& value,
                           const std::vector<std::string>& choices);

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENEFORMATTING_HPP
