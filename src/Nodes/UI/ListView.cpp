#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Nodes/UI/ListView.hpp>

namespace guayaba {

ListView::ListView(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    // Declared here so connect_signal finds them. Both carry the row index.
    declare_signal<int>("selection_changed");
    declare_signal<int>("item_activated");

    // Reachable by keyboard: clicking focuses it, and Tab reaches it.
    set_focus_mode(FocusMode::CLICK);
}

// --- Size -------------------------------------------------------------------

Vec2 ListView::get_minimum_size() const {
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();
    return {own.x + frame.x, std::max(own.y, 1 + frame.y)};
}

Rect ListView::content_rect() const { return stylebox.get_content_rect(get_rect()); }

int ListView::visible_count() const { return content_rect().size.y; }

// --- Items ------------------------------------------------------------------

void ListView::set_items(std::vector<std::string> new_items) {
    items = std::move(new_items);

    // Both can now point past the end: the cursor is clamped rather than cleared,
    // and the window is clamped to the new count.
    if (items.empty()) {
        selected_index = -1;
    } else {
        if (selected_index < -1) selected_index = -1;
        if (selected_index >= static_cast<int>(items.size())) {
            selected_index = static_cast<int>(items.size()) - 1;
        }
    }
    clamp_scroll();
}

// --- Selection --------------------------------------------------------------

void ListView::set_selected(int index) {
    int target = index;
    if (target < 0) {
        target = -1;
    } else if (items.empty()) {
        target = -1;
    } else if (target >= static_cast<int>(items.size())) {
        target = static_cast<int>(items.size()) - 1;
    }
    apply_selection(target);
}

void ListView::select_first() {
    if (!items.empty()) apply_selection(0);
}

void ListView::select_last() {
    if (!items.empty()) apply_selection(static_cast<int>(items.size()) - 1);
}

void ListView::move_selection(int delta) {
    if (items.empty() || delta == 0) return;

    const int last = static_cast<int>(items.size()) - 1;
    int target;
    if (selected_index < 0) {
        // Nothing selected: the first move enters from the end it moves toward.
        target = delta > 0 ? 0 : last;
    } else {
        target = std::clamp(selected_index + delta, 0, last);
    }

    apply_selection(target);
}

void ListView::apply_selection(int index) {
    if (index == selected_index) return;

    selected_index = index;
    ensure_cursor_visible();
    emit_signal("selection_changed", selected_index);
}

void ListView::ensure_cursor_visible() {
    if (selected_index < 0) return;

    const int visible = visible_count();
    if (visible <= 0) return;

    if (selected_index < scroll_offset) {
        scroll_offset = selected_index;
    } else if (selected_index >= scroll_offset + visible) {
        scroll_offset = selected_index - visible + 1;
    }
    clamp_scroll();
}

void ListView::clamp_scroll() {
    const int count = static_cast<int>(items.size());
    const int max_offset = std::max(0, count - visible_count());
    scroll_offset = std::clamp(scroll_offset, 0, max_offset);
}

void ListView::set_scroll(int offset) {
    scroll_offset = offset;
    clamp_scroll();
}

// --- Appearance -------------------------------------------------------------

void ListView::set_stylebox(StyleBox box) {
    stylebox = std::move(box);
    // The frame is part of the content rect, so the visible-row count can change
    // and the window must follow.
    clamp_scroll();
}

void ListView::set_text_color(const std::string& color_name) {
    text_color = Color::from_name(color_name);
}

void ListView::set_selection_background_color(const std::string& color_name) {
    selection_background_color = Color::from_name(color_name);
}

void ListView::set_selection_text_color(const std::string& color_name) {
    selection_text_color = Color::from_name(color_name);
}

void ListView::set_item_color(int index, uint16_t color) {
    if (index < 0) return;
    item_colors[index] = color;
}

void ListView::clear_item_colors() { item_colors.clear(); }

// --- Input ------------------------------------------------------------------

bool ListView::on_gui_input(const InputEvent& event) {
    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        if (event.mouse_button != MouseButton::LEFT) return false;

        if (!event.pressed) {
            // The release is swallowed with the press so a click cannot half-leak
            // to a control behind.
            return true;
        }

        // Focus is taken here because the engine does not do it for a click.
        grab_focus();

        const Rect content = content_rect();
        if (!content.is_empty() && content.contains(event.mouse_pos)) {
            const int row = event.mouse_pos.y - content.pos.y + scroll_offset;
            if (row >= 0 && row < static_cast<int>(items.size())) {
                apply_selection(row);
            }
        }
        return true;
    }

    if (event.type == InputEvent::Type::MOUSE_WHEEL) {
        // Decline when there is nothing to scroll, so an enclosing ScrollView gets
        // the notch.
        if (static_cast<int>(items.size()) <= visible_count()) return false;

        // Roughly a third of a screen per notch, at least one row. Positive wheel
        // moves toward the top.
        const int step = std::max(1, visible_count() / 3);
        set_scroll(scroll_offset - event.wheel_delta * step);
        return true;
    }

    // Keys arrive only while focused: Control gates KEY events by is_focused().
    if (event.type != InputEvent::Type::KEY || !event.pressed) return false;

    switch (event.key) {
        case Key::UP:
            move_selection(-1);
            return true;

        case Key::DOWN:
            move_selection(1);
            return true;

        case Key::PAGE_UP:
            move_selection(-std::max(1, visible_count() - 1));
            return true;

        case Key::PAGE_DOWN:
            move_selection(std::max(1, visible_count() - 1));
            return true;

        case Key::HOME:
            select_first();
            return true;

        case Key::END:
            select_last();
            return true;

        case Key::ENTER:
            if (selected_index >= 0) emit_signal("item_activated", selected_index);
            return true;

        // Declined so the scene keeps them: Tab moves focus, Escape cancels.
        case Key::TAB:
        case Key::ESCAPE:
            return false;

        default:
            return false;
    }
}

// --- Drawing ----------------------------------------------------------------

void ListView::draw_row(ConsoleRenderer& renderer, const Rect& content, int row,
                        const std::string& item, uint16_t fg, uint16_t bg,
                        const Rect& clip) {
    const int y = content.pos.y + row;

    // The whole row, not just the text: the selection band spans the content rect
    // and the empty cells past a short item are part of it. Drawn per cell.
    std::size_t byte = 0;
    for (int column = 0; column < content.size.x; ++column) {
        wchar_t glyph = L' ';
        if (byte < item.size()) {
            char32_t codepoint = 0;
            const std::size_t length = decode_utf8(item, byte, codepoint);
            glyph = static_cast<wchar_t>(codepoint);
            byte += length;
        }
        renderer.put_char(Vec2{content.pos.x + column, y}, glyph, fg, bg, clip);
    }
}

void ListView::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    // Drawn exactly as large as it is, clipped only by its ancestors.
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::on_draw(renderer, clip);
        return;
    }

    // A StyleBox background is the row surface when it has one; otherwise the
    // TermChar default.
    const uint16_t row_background =
        stylebox.has_background() ? *stylebox.get_background_color() : 0;

    const int count = static_cast<int>(items.size());
    for (int row = 0; row < content.size.y; ++row) {
        const int index = scroll_offset + row;
        if (index >= count) break;  // the window can be taller than the list

        const bool is_selected = index == selected_index;
        const auto tint = item_colors.find(index);

        // Selection wins over a per-item tint, so the cursor band stays legible.
        const uint16_t fg = is_selected ? selection_text_color
                          : tint != item_colors.end() ? tint->second
                                                      : text_color;
        const uint16_t bg = is_selected ? selection_background_color : row_background;

        draw_row(renderer, content, row, items[index], fg, bg, clip);
    }

    // Hand back to Control so the script hook runs and children use the clip
    // Control narrows for them.
    Control::on_draw(renderer, clip);
}

} // namespace guayaba
