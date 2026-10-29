#include <utility>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

Theme& Theme::get_default() {
    static Theme instance;
    return instance;
}

void Theme::set_style(const std::string& key, StyleBox style) {
    styles[key] = std::move(style);
    ++revision_;
}

void Theme::set_color(const std::string& key, uint16_t color) {
    colors[key] = color;
    ++revision_;
}

const StyleBox& Theme::get_style(const std::string& key, const StyleBox& fallback) const {
    const auto found = styles.find(key);
    return found != styles.end() ? found->second : fallback;
}

uint16_t Theme::get_color(const std::string& key, uint16_t fallback) const {
    const auto found = colors.find(key);
    return found != colors.end() ? found->second : fallback;
}

bool Theme::has_style(const std::string& key) const {
    return styles.find(key) != styles.end();
}

bool Theme::has_color(const std::string& key) const {
    return colors.find(key) != colors.end();
}

void apply_theme(Node& root, const Theme& theme) {
    if (Control* control = dynamic_cast<Control*>(&root)) {
        control->apply_theme(theme);
    }

    // Re-read the count each pass: an override may add or remove children, and
    // get_child() throws on an index that no longer exists.
    for (size_t i = 0; i < root.get_child_count(); ++i) {
        Node* child = root.get_child<Node>(i).get();
        if (child != nullptr) apply_theme(*child, theme);
    }
}

} // namespace guayaba
