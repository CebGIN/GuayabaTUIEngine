#ifndef GUAYABA_SCENE_SCENEVALIDATION_HPP
#define GUAYABA_SCENE_SCENEVALIDATION_HPP

#include <string>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba {

/*
    Checks structure, names, duplicates, type names, property names and value
    shapes without building a tree. The loader runs it in strict mode; the editor
    can run it in non-strict mode so unknown types and properties become warnings
    that do not prevent saving.

    Style references are resolved by build_scene(), because that is where the
    theme is available.
*/
std::vector<SceneDiagnostic> validate_scene(const SceneDocument& document,
                                            bool strict = true,
                                            const std::string& source = "<memory>");

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEVALIDATION_HPP
