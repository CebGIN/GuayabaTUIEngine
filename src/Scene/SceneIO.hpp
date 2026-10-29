#ifndef GUAYABA_SCENE_SCENEIO_HPP
#define GUAYABA_SCENE_SCENEIO_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace guayaba::detail {

struct TextFile {
    bool ok = false;
    std::string text;
    std::string error;
};

TextFile read_text_file(const std::filesystem::path& path, std::size_t max_bytes);
bool write_text_file(const std::filesystem::path& path, std::string_view text,
                     std::string* error = nullptr);

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENEIO_HPP
