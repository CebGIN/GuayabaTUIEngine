#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Utf8.hpp>
#include <guayaba/Nodes/UI/LineEdit.hpp>

namespace guayaba {

namespace {

// A continuation byte is 10xxxxxx; every other byte starts a codepoint. Forward
// and backward walks share this test, which keeps them agreeing on boundaries.
bool is_continuation(unsigned char byte) { return (byte & 0xC0) == 0x80; }

// The UTF-8 bytes for one codepoint, handed straight to std::string::insert.
std::string encode(char32_t codepoint) {
    std::string out;
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return out;
}

// Byte offset one codepoint after index, clamped to size().
std::size_t next_offset(const std::string& value, std::size_t index) {
    if (index >= value.size()) return value.size();
    std::size_t i = index + 1;
    while (i < value.size() && is_continuation(static_cast<unsigned char>(value[i]))) ++i;
    return i;
}

// Byte offset one codepoint before index.
std::size_t previous_offset(const std::string& value, std::size_t index) {
    if (index == 0) return 0;
    std::size_t i = index - 1;
    while (i > 0 && is_continuation(static_cast<unsigned char>(value[i]))) --i;
    return i;
}

// Codepoints in value[0, end); the caret column is this count for the caret byte
// offset.
std::size_t count_codepoints(const std::string& value, std::size_t end) {
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < end && i < value.size()) {
        i = next_offset(value, i);
        ++count;
    }
    return count;
}

std::size_t count_codepoints(const std::string& value) {
    return count_codepoints(value, value.size());
}

// Byte offset of the count-th codepoint, clamped to size().
std::size_t codepoint_offset(const std::string& value, std::size_t count) {
    std::size_t i = 0;
    while (count > 0 && i < value.size()) {
        i = next_offset(value, i);
        --count;
    }
    return i;
}

} // namespace

LineEdit::LineEdit(const std::string& node_name, Vec2 node_position)
    : Control(node_name, node_position) {
    // Declared here so connect_signal finds them. Both carry the current text.
    declare_signal<std::string>("text_changed");
    declare_signal<std::string>("text_submitted");

    // Reachable by keyboard: clicking focuses it, and Tab reaches it.
    set_focus_mode(FocusMode::CLICK);
}

// --- Size -------------------------------------------------------------------

Vec2 LineEdit::get_minimum_size() const {
    // Always one line tall plus the frame; a taller caller minimum still wins.
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();
    return {own.x + frame.x, std::max(own.y, 1 + frame.y)};
}

Rect LineEdit::content_rect() const { return stylebox.get_content_rect(get_rect()); }

// --- Editing ----------------------------------------------------------------

void LineEdit::clamp_caret() {
    if (caret > text.size()) caret = text.size();

    // Steps back off a continuation byte, keeping the caret on a boundary.
    while (caret > 0 && caret < text.size() &&
           is_continuation(static_cast<unsigned char>(text[caret]))) {
        --caret;
    }
}

void LineEdit::commit_text(std::string new_text) {
    // The equality gate makes "text_changed" mean the value moved: an identical
    // re-set is silent.
    if (new_text == text) return;

    text = std::move(new_text);
    clamp_caret();
    emit_signal("text_changed", text);
}

bool LineEdit::insert_codepoint(char32_t codepoint) {
    // The limit counts codepoints, not bytes. Once full the character is dropped
    // but the key is still consumed.
    if (max_length > 0 &&
        static_cast<int>(count_codepoints(text)) >= max_length) {
        return false;
    }

    const std::string encoded = encode(codepoint);
    std::string updated = text;
    updated.insert(caret, encoded);
    caret += encoded.size();
    commit_text(std::move(updated));
    return true;
}

bool LineEdit::backspace() {
    if (caret == 0) return false;

    const std::size_t start = previous_offset(text, caret);
    std::string updated = text;
    updated.erase(start, caret - start);
    caret = start;
    commit_text(std::move(updated));
    return true;
}

bool LineEdit::delete_forward() {
    if (caret >= text.size()) return false;

    const std::size_t end = next_offset(text, caret);
    std::string updated = text;
    updated.erase(caret, end - caret);
    commit_text(std::move(updated));
    return true;
}

void LineEdit::set_text(const std::string& new_text) { commit_text(new_text); }

void LineEdit::clear() {
    caret = 0;
    commit_text(std::string());
}

void LineEdit::set_caret(std::size_t index) {
    caret = index;
    clamp_caret();
}

void LineEdit::set_stylebox(StyleBox box) { stylebox = std::move(box); }

void LineEdit::set_text_color(const std::string& color_name) {
    text_color = Color::from_name(color_name);
}

void LineEdit::set_placeholder_color(const std::string& color_name) {
    placeholder_color = Color::from_name(color_name);
}

void LineEdit::set_background_color(const std::string& color_name) {
    background_color = Color::from_name(color_name);
}

// --- Input ------------------------------------------------------------------

bool LineEdit::on_gui_input(const InputEvent& event) {
    if (event.type == InputEvent::Type::MOUSE_BUTTON) {
        // Focus is taken here because the engine does not do it for a click.
        // Consuming it keeps the press from reaching behind.
        if (event.pressed && event.mouse_button == MouseButton::LEFT) {
            grab_focus();
            return true;
        }
        return false;
    }

    // Keys below arrive only while focused: Control::can_receive_input gates KEY
    // events by is_focused().
    if (event.type != InputEvent::Type::KEY || !event.pressed) return false;

    switch (event.key) {
        case Key::ENTER:
            emit_signal("text_submitted", text);
            return true;

        case Key::BACKSPACE:
            backspace();
            return true;

        case Key::DELETE:
            delete_forward();
            return true;

        case Key::LEFT:
            caret = previous_offset(text, caret);
            return true;

        case Key::RIGHT:
            caret = next_offset(text, caret);
            return true;

        case Key::HOME:
            caret = 0;
            return true;

        case Key::END:
            caret = text.size();
            return true;

        // Declined, not consumed: Tab must reach FocusManager and Escape the scene.
        case Key::TAB:
        case Key::ESCAPE:
            return false;

        default:
            break;
    }

    // A printable codepoint. The upper test excludes DEL; the ctrl test keeps
    // control combinations from typing a control character.
    if (event.text >= 0x20 && event.text != 0x7f && !event.ctrl) {
        insert_codepoint(event.text);
        return true;
    }

    return false;
}

// --- Drawing ----------------------------------------------------------------

void LineEdit::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // Drawn exactly as large as it is, clipped only by its ancestors.
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    const int width = content.size.x;
    const int origin_x = content.pos.x;
    const int row = content.pos.y;

    // The field paints every visible cell, so the surface is uniform and the caret
    // block is solid. A StyleBox background wins over the field's own.
    const uint16_t background = stylebox.has_background()
                                    ? *stylebox.get_background_color()
                                    : background_color;

    const bool empty = text.empty();
    const bool focused = is_focused();

    // Empty shows the hint in its own colour and puts the caret at column zero.
    const std::string& source = empty ? placeholder : text;
    const std::size_t caret_column =
        empty ? 0 : count_codepoints(text, caret);

    // Scroll only as far as needed to keep the caret visible. The caret is a cell
    // of its own past the last character, so the test is caret_column + 1 > width.
    std::size_t first = 0;
    if (!empty && caret_column + 1 > static_cast<std::size_t>(width)) {
        first = caret_column + 1 - static_cast<std::size_t>(width);
    }

    std::size_t byte = codepoint_offset(source, first);

    for (int column = 0; column < width; ++column) {
        const std::size_t index = first + static_cast<std::size_t>(column);
        const bool is_caret =
            focused && (empty ? column == 0 : index == caret_column);

        wchar_t glyph = L' ';
        uint16_t foreground = empty ? placeholder_color : text_color;

        if (byte < source.size()) {
            char32_t codepoint = 0;
            const std::size_t length = decode_utf8(source, byte, codepoint);
            // Secret mode substitutes the glyph, one per visible codepoint, so the
            // width still matches. The stored text is untouched.
            glyph = (secret && !empty) ? static_cast<wchar_t>(secret_character)
                                       : static_cast<wchar_t>(codepoint);
            byte += length;
        }

        uint16_t cell_foreground = foreground;
        uint16_t cell_background = background;
        if (is_caret) {
            // Reverse video: the pair is swapped, so the caret is a block in the
            // text colour.
            cell_foreground = background;
            cell_background = text_color;
        }

        // Per cell: the glyph and colours depend on the caret's position and on
        // secret mode.
        renderer.put_char(Vec2{origin_x + column, row}, glyph, cell_foreground,
                          cell_background, clip);
    }

    // Hand back to Control so the script hook runs and children use the clip
    // Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba
