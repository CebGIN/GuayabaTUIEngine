#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Nodes/UI/ListView.hpp>

namespace guayaba {

namespace {

// One UTF-8 codepoint at byte `index`, with the number of bytes it spans. A
// truncated or malformed sequence is reported as a single byte so a bad row still
// renders instead of derailing the rest of the line.
//
// This mirrors LineEdit's private decoder: the caret there needs to walk the same
// boundaries. It is repeated rather than shared because the two controls are in
// separate translation units and the helper is small; a row is drawn per cell, so
// the bytes must be turned into codepoints here rather than handed to put_text,
// which writes one byte per cell.
std::size_t decode_utf8(const std::string& value, std::size_t index, char32_t& codepoint) {
    const unsigned char lead = static_cast<unsigned char>(value[index]);

    std::size_t length = 1;
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
    }

    if (index + length > value.size()) {
        codepoint = lead;
        return 1;
    }

    char32_t decoded = (length == 1) ? lead
                     : (length == 2) ? (lead & 0x1F)
                     : (length == 3) ? (lead & 0x0F)
                                     : (lead & 0x07);
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char continuation =
            static_cast<unsigned char>(value[index + i]);
        if ((continuation & 0xC0) != 0x80) {
            codepoint = lead;  // malformed: emit the lead alone
            return 1;
        }
        decoded = (decoded << 6) | (continuation & 0x3F);
    }

    codepoint = decoded;
    return length;
}

} // namespace

ListView::ListView(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    // Declared here rather than on demand at the emit, so connect_signal finds
    // them before anything has fired. Both carry the row index.
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

    // Both can now point past the end. The cursor is clamped rather than cleared,
    // so replacing the contents of a live list keeps the user near where they
    // were; the window is clamped to the new count.
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
        // Nothing selected: the first move enters the list from the end it moves
        // toward, which is what makes Down-then-Up feel symmetric on an empty
        // selection.
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
    // The frame is part of the content rect, so the number of visible rows can
    // change with it and the window has to follow.
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
            // The press was taken; the release is swallowed with it so a click
            // cannot half-leak to a control behind the list.
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
        // Decline when there is nothing to scroll, so an enclosing ScrollView
        // still gets the notch.
        if (static_cast<int>(items.size()) <= visible_count()) return false;

        // Roughly a third of a screen per notch, at least one row. Positive wheel
        // is up, which moves the window toward the top (a smaller offset).
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

    // The whole row, not just the text: the selection band has to span the content
    // rect, and the empty cells past a short item are part of the band. Drawn per
    // cell because each cell's colour depends on the row it is in.
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

void ListView::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // The list is drawn exactly as large as it is and is clipped only by what its
    // ancestors allowed.
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    // A StyleBox background is the row surface when it has one; otherwise the
    // TermChar default black.
    const uint16_t row_background =
        stylebox.has_background() ? *stylebox.get_background_color() : 0;

    const int count = static_cast<int>(items.size());
    for (int row = 0; row < content.size.y; ++row) {
        const int index = scroll_offset + row;
        if (index >= count) break;  // the window can be taller than the list

        const bool is_selected = index == selected_index;
        const auto tint = item_colors.find(index);

        // Selection wins over a per-item tint, so the cursor band stays legible
        // even on a row that was coloured for another reason.
        const uint16_t fg = is_selected ? selection_text_color
                          : tint != item_colors.end() ? tint->second
                                                      : text_color;
        const uint16_t bg = is_selected ? selection_background_color : row_background;

        draw_row(renderer, content, row, items[index], fg, bg, clip);
    }

    // Hand back to Control so the script hook still runs and children are drawn
    // with the clip Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba
