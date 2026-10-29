#ifndef GUAYABA_RESOURCES_THEME_HPP
#define GUAYABA_RESOURCES_THEME_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

class Node;

/*
    A set of StyleBoxes and colours addressed by key, plus a revision counter.

    The default instance is empty, so every lookup returns the caller's fallback
    and the engine's appearance is unchanged until a theme is filled in. There is
    no automatic application: the caller decides when to run apply_theme().

    The canonical keys are the ones in the specification, plus the extensions the
    existing controls need:

        panel, button.normal, button.hover, button.pressed, button.disabled,
        line_edit, list_view, progress_bar, check_box, spin_box, split_container,
        text.primary, text.secondary, text.disabled,
        selection.background, selection.text

        label, spinner,
        progress_bar.fill, progress_bar.background, progress_bar.text,
        split_container.divider

    table.*, tab_container.* and option_button.* are reserved until those
    controls exist.
*/
class Theme {
public:
    static Theme& get_default();

    Theme() = default;
    Theme(const Theme&) = delete;
    Theme& operator=(const Theme&) = delete;

    void set_style(const std::string& key, StyleBox style);
    void set_color(const std::string& key, uint16_t color);

    /*
        The stored style, or `fallback` when the key is absent. The reference is
        to one of the two, so the fallback must outlive the call.
    */
    const StyleBox& get_style(const std::string& key, const StyleBox& fallback) const;

    uint16_t get_color(const std::string& key, uint16_t fallback) const;

    bool has_style(const std::string& key) const;
    bool has_color(const std::string& key) const;

    // Bumped by every set_style()/set_color(). No auto-application is tied to it.
    std::uint64_t revision() const { return revision_; }

private:
    std::unordered_map<std::string, StyleBox> styles;
    std::unordered_map<std::string, uint16_t> colors;
    std::uint64_t revision_ = 0;
};

// Depth-first: each Control's apply_theme(), then its children.
void apply_theme(Node& root, const Theme& theme);

} // namespace guayaba

#endif // GUAYABA_RESOURCES_THEME_HPP
