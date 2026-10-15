#include <algorithm>
#include <utility>

#include <guayaba/Core/Color.hpp>
#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Nodes/UI/LineEdit.hpp>

namespace guayaba {

namespace {

// A continuation byte is 10xxxxxx; every other byte starts a codepoint. This one
// test is what makes walking the string forwards and backwards agree on where the
// codepoint boundaries are, which is what keeps the caret valid.
bool is_continuation(unsigned char byte) { return (byte & 0xC0) == 0x80; }

// How many bytes the codepoint starting at index spans, at least one. A truncated
// or malformed sequence is reported as a single byte rather than desyncing the
// walk: a field has to render whatever set_text was handed, and giving up on the
// rest of the line would be worse than showing one replacement-shaped cell.
std::size_t sequence_length(const std::string& value, std::size_t index) {
    const unsigned char lead = static_cast<unsigned char>(value[index]);

    std::size_t length = 1;
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
    }

    if (index + length > value.size()) return 1;
    for (std::size_t i = 1; i < length; ++i) {
        if (!is_continuation(static_cast<unsigned char>(value[index + i]))) return 1;
    }
    return length;
}

char32_t decode_at(const std::string& value, std::size_t index, std::size_t& length) {
    length = sequence_length(value, index);
    const unsigned char lead = static_cast<unsigned char>(value[index]);
    if (length == 1) return lead;

    char32_t codepoint = (length == 2) ? (lead & 0x1F)
                       : (length == 3) ? (lead & 0x0F)
                                       : (lead & 0x07);
    for (std::size_t i = 1; i < length; ++i) {
        codepoint = (codepoint << 6) |
                    (static_cast<unsigned char>(value[index + i]) & 0x3F);
    }
    return codepoint;
}

// The UTF-8 bytes for one codepoint. The insert path hands the result straight to
// std::string::insert, so this is the only place bytes are built rather than read.
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

// The byte offset one codepoint after index, clamped to size(). This is "move the
// caret right" at the byte level.
std::size_t next_offset(const std::string& value, std::size_t index) {
    if (index >= value.size()) return value.size();
    std::size_t i = index + 1;
    while (i < value.size() && is_continuation(static_cast<unsigned char>(value[i]))) ++i;
    return i;
}

// The byte offset one codepoint before index. This is "move the caret left" and
// "delete the character behind the caret" at the byte level.
std::size_t previous_offset(const std::string& value, std::size_t index) {
    if (index == 0) return 0;
    std::size_t i = index - 1;
    while (i > 0 && is_continuation(static_cast<unsigned char>(value[i]))) --i;
    return i;
}

// Codepoints in value[0, end). The caret column is this count for the caret byte
// offset, since one codepoint is one cell on the grid.
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

// Byte offset of the count-th codepoint, clamped to size(). Used to find the first
// byte of the visible window from its column.
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
    // Declared here rather than on demand at the emit, so connect_signal finds
    // them before anything has fired and so the two are the signals a LineEdit is
    // known to have. Both carry the current text, which is what a listener acts on.
    declare_signal<std::string>("text_changed");
    declare_signal<std::string>("text_submitted");

    // Reachable by keyboard: clicking focuses it, and Tab reaches it. Taking the
    // focus on click is done in on_gui_input, because the engine does not
    // auto-focus - see the class comment.
    set_focus_mode(FocusMode::CLICK);
}

// --- Size -------------------------------------------------------------------

Vec2 LineEdit::get_minimum_size() const {
    // Always one line tall: a field that could be laid out two rows tall would
    // draw its text in the first and leave the rest as a mystery band. A taller
    // caller minimum still wins - this is a floor, and the frame comes off it so
    // the text sits inside the border rather than under it.
    const Vec2 frame = stylebox.get_frame_size();
    const Vec2 own = Control::get_minimum_size();
    return {own.x + frame.x, std::max(own.y, 1 + frame.y)};
}

Rect LineEdit::content_rect() const { return stylebox.get_content_rect(get_rect()); }

// --- Editing ----------------------------------------------------------------

void LineEdit::clamp_caret() {
    if (caret > text.size()) caret = text.size();

    // Stepping back off a continuation byte is what keeps the caret on a boundary
    // after an index was clamped down or set from outside inside a codepoint.
    while (caret > 0 && caret < text.size() &&
           is_continuation(static_cast<unsigned char>(text[caret]))) {
        --caret;
    }
}

void LineEdit::commit_text(std::string new_text) {
    // The equality gate is why "text_changed" can be trusted: it means the value
    // moved, not that something asked it to. A re-typed identical value is silent.
    if (new_text == text) return;

    text = std::move(new_text);
    clamp_caret();
    emit_signal("text_changed", text);
}

bool LineEdit::insert_codepoint(char32_t codepoint) {
    // Codepoints, not bytes: the limit is the count the user sees. Once full the
    // character is dropped, but the key is still consumed - letting it fall
    // through would make it act on whatever is behind the field.
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
        // Focus is taken here because the engine does not do it for a click; a
        // field is the control where that gesture means nothing else. Consuming it
        // keeps the press from also reaching whatever sits behind.
        if (event.pressed && event.mouse_button == MouseButton::LEFT) {
            grab_focus();
            return true;
        }
        return false;
    }

    // Keys below this line only ever arrive while focused: Control::can_receive_input
    // gates KEY events by is_focused(), so there is no check to repeat here.
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

        // Declined, not consumed: Tab has to reach FocusManager to move on, and
        // Escape is the scene's to cancel with. Swallowing either here would make
        // a field trap the keyboard.
        case Key::TAB:
        case Key::ESCAPE:
            return false;

        default:
            break;
    }

    // A printable codepoint. The upper test excludes DEL, and the ctrl test keeps
    // control combinations (which some terminals deliver as a low text byte) from
    // typing a control character.
    if (event.text >= 0x20 && event.text != 0x7f && !event.ctrl) {
        insert_codepoint(event.text);
        return true;
    }

    return false;
}

// --- Drawing ----------------------------------------------------------------

void LineEdit::draw(ConsoleRenderer& renderer, const Rect& clip) {
    // The field is drawn exactly as large as it is and is clipped only by what its
    // ancestors allowed: clip_children governs what this node contains, not
    // whether the node itself is visible.
    stylebox.apply(renderer, get_rect(), clip);

    const Rect content = content_rect();
    if (content.is_empty()) {
        Control::draw(renderer, clip);
        return;
    }

    const int width = content.size.x;
    const int origin_x = content.pos.x;
    const int row = content.pos.y;

    // The field owns its content band: every visible cell is painted, so the
    // surface is uniform whatever the StyleBox filled, and the caret block is a
    // solid cell rather than a gap. A StyleBox background wins over the field's
    // own, the way Button lets its box decide the band under the text.
    const uint16_t background = stylebox.has_background()
                                    ? *stylebox.get_background_color()
                                    : background_color;

    const bool empty = text.empty();
    const bool focused = is_focused();

    // Empty shows the hint in its own colour and puts the caret at column zero.
    const std::string& source = empty ? placeholder : text;
    const std::size_t caret_column =
        empty ? 0 : count_codepoints(text, caret);

    // The window scrolls only as far as it must to keep the caret visible. The
    // caret is a cell of its own - at the end of the line it sits past the last
    // character - so the condition is caret_column + 1 > width rather than
    // caret_column >= width, and a line that fills the field exactly still scrolls
    // by one to make room for it. While the caret fits, first stays zero.
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
            std::size_t length = 0;
            const char32_t codepoint = decode_at(source, byte, length);
            // Secret mode substitutes the glyph, one per visible codepoint, so the
            // masked width still matches what was typed. The stored text is not
            // touched - get_text() still returns it.
            glyph = (secret && !empty) ? static_cast<wchar_t>(secret_character)
                                       : static_cast<wchar_t>(codepoint);
            byte += length;
        }

        uint16_t cell_foreground = foreground;
        uint16_t cell_background = background;
        if (is_caret) {
            // Reverse video: the pair is swapped, so the caret is a block in the
            // text colour with the character knocked out of it. A space at the end
            // of the line becomes a plain block.
            cell_foreground = background;
            cell_background = text_color;
        }

        // Per cell, not a batch: the glyph and the colours both depend on the
        // caret's position and on secret mode, which is exactly the case
        // ConsoleRenderer::put_char documents its escape hatch for.
        renderer.put_char(Vec2{origin_x + column, row}, glyph, cell_foreground,
                          cell_background, clip);
    }

    // Hand back to Control so the script hook still runs and children are drawn
    // with the clip Control narrows for them.
    Control::draw(renderer, clip);
}

} // namespace guayaba
