#ifndef GUAYABA_SCENE_SCENEWRITER_HPP
#define GUAYABA_SCENE_SCENEWRITER_HPP

#include <filesystem>
#include <string>

#include <guayaba/Scene/SceneFormat.hpp>

namespace guayaba {

struct SceneWriteOptions {
    // Also emit absent schema properties with their effective defaults. Explicit
    // properties are always written, even when their value equals the default: a
    // theme is applied before them, so dropping an explicit default can change the
    // resulting appearance. Materialising defaults has the same consequence: a
    // default style or colour emitted here overrides a theme on reload.
    bool include_defaults = false;
    int indent = 4;
    bool trailing_newline = true;
};

/*
    Serialises the AST. It does not validate property values: the editor must be
    able to save a document with an error under repair. It does require a root,
    valid names and no duplicate siblings.

    Comments are not part of the AST and therefore do not survive a save.
*/
std::string write_scene(const SceneDocument& document,
                        const SceneWriteOptions& options = {});

void write_scene_file(const SceneDocument& document,
                      const std::filesystem::path& path,
                      const SceneWriteOptions& options = {});

} // namespace guayaba

#endif // GUAYABA_SCENE_SCENEWRITER_HPP
