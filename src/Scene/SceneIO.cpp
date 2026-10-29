#include <fstream>
#include <string>

#include "SceneIO.hpp"

namespace guayaba::detail {

TextFile read_text_file(const std::filesystem::path& path, std::size_t max_bytes) {
    TextFile result;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "cannot open file '" + path.string() + "'";
        return result;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) {
        result.error = "cannot determine the size of '" + path.string() + "'";
        return result;
    }
    if (static_cast<std::size_t>(size) > max_bytes) {
        result.error = "file '" + path.string() + "' is larger than the configured maximum (" +
                       std::to_string(max_bytes) + " bytes)";
        return result;
    }
    file.seekg(0, std::ios::beg);

    result.text.resize(static_cast<std::size_t>(size));
    if (size > 0) file.read(result.text.data(), size);
    if (!file && !file.eof()) {
        result.error = "cannot read file '" + path.string() + "'";
        result.text.clear();
        return result;
    }

    result.ok = true;
    return result;
}

bool write_text_file(const std::filesystem::path& path, std::string_view text,
                     std::string* error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (error) *error = "cannot open file '" + path.string() + "' for writing";
        return false;
    }

    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file) {
        if (error) *error = "cannot write file '" + path.string() + "'";
        return false;
    }
    return true;
}

} // namespace guayaba::detail
