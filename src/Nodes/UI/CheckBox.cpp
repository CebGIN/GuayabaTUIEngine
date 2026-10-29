#include <algorithm>
#include <stdexcept>
#include <string>

#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Nodes/UI/CheckBox.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

namespace {

// The engine measures text by codepoint today, so a single char32_t is one cell.
// Control characters and NUL are still a configuration error; real East Asian
// width arrives with CORE-04.
bool is_printable_codepoint(char32_t codepoint) {
    return codepoint >= 0x20 && codepoint != 0x7F;
}

} // namespace

CheckBox::CheckBox(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    declare_signal<bool>("toggled");

    // Reachable by keyboard: clicking focuses it, and Tab reaches it.
    set_focus_mode(FocusMode::CLICK);
}

void CheckBox::set_checked(bool value) {
    if (checked == value) return;

    checked = value;
    emit_signal("toggled", checked);
}

void CheckBox::set_glyphs(char32_t checked_glyph, char32_t unchecked_glyph) {
    if (!is_printable_codepoint(checked_glyph) || !is_printable_codepoint(unchecked_glyph)) {
        throw std::invalid_argument(
            "CheckBox::set_glyphs: glyphs must be printable single cells.");
    }

    checked_mark = checked_glyph;
    unchecked_mark = unchecked_glyph;
}

Rect CheckBox::content_rect() const {
    return stylebox.get_content_rect(get_rect());
}

Vec2 CheckBox::get_minimum_size() const {
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();

    // "[x]" is three columns; the separating space only exists with text.
    const int width = 3 + (text.empty() ? 0 : 1 + text_width(text));
    return {
        std::max(own.x, frame.x + width),
        std::max(own.y, frame.y + 1),
    };
}

void CheckBox::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (!content.is_empty()) {
        const Rect inner = content.intersection(clip);
        const int y = content.pos.y;

        renderer.put_char({content.pos.x, y}, L'[', text_color, background_color, inner);
        renderer.put_char({content.pos.x + 1, y},
                          static_cast<wchar_t>(checked ? checked_mark : unchecked_mark),
                          text_color, background_color, inner);
        renderer.put_char({content.pos.x + 2, y}, L']', text_color, background_color, inner);

        if (!text.empty()) {
            renderer.put_text({content.pos.x + 4, y}, {text}, text_color, background_color, inner);
        }
    }

    Control::on_draw(renderer, clip);
}

bool CheckBox::on_gui_input(const InputEvent& event) {
    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        if (event.pressed && event.mouse_button == MouseButton::LEFT &&
            is_inside(event.mouse_pos)) {
            grab_focus();
            toggle();
            return true;
        }
        return false;
    }

    if (event.type == InputEvent::Type::KEY && event.pressed) {
        // Keys only reach a focused control, so no extra focus check is needed.
        if (event.key == Key::SPACE || event.key == Key::ENTER) {
            toggle();
            return true;
        }
    }

    return false;
}

void CheckBox::apply_theme(const Theme& theme) {
    stylebox = theme.get_style("check_box", stylebox);
    text_color = theme.get_color("text.primary", text_color);
}

} // namespace guayaba
