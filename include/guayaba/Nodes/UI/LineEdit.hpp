#ifndef GUAYABA_NODES_UI_LINEEDIT_HPP
#define GUAYABA_NODES_UI_LINEEDIT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A Control that edits one line of text inside a StyleBox.

        +--------------------+
        | user_name_|        |
        +--------------------+

    Focus is process-wide (FocusManager). The field declares FocusMode::CLICK and
    takes focus itself when a left press lands on it; the engine does not
    auto-focus. Once focused, KEY events reach on_gui_input; TAB and ESCAPE are
    declined so the scene can use them.

    The caret is a byte offset into the UTF-8 text. It is kept on a codepoint
    boundary and clamped inside the text; display columns are counted by decoding.
    get_caret()/set_caret() speak bytes.

    max_length counts codepoints, not bytes, and stops inserts once full without
    truncating text set through set_text.

    set_secret(true) draws one secret_character per visible codepoint; the stored
    text is unchanged.

    Signals: "text_changed" (std::string) fires when the stored text actually
    changes, "text_submitted" (std::string) on ENTER. Both carry the current text.
*/
class LineEdit : public Control {
protected:
    StyleBox stylebox;

    std::string text;
    std::string placeholder;

    // Byte offset into text, always on a codepoint boundary and never past size().
    std::size_t caret = 0;

    // 0 or less means no limit, counted in codepoints.
    int max_length = 0;

    bool secret = false;
    char32_t secret_character = U'*';

    uint16_t text_color = 7;             // White, the TermChar default
    uint16_t placeholder_color = 8;      // BRIGHT_BLACK, so the hint reads as a hint
    uint16_t background_color = 0;       // Black, the TermChar default

    // The box's rect minus its border and margins; empty when the box is too small.
    Rect content_rect() const;

    // A left press takes focus; editing keys are handled while the field holds it.
    // Declines TAB and ESCAPE.
    bool on_gui_input(const InputEvent& event) override;

    // Snaps the caret to a codepoint lead byte inside the text. Every path that
    // changes text or caret goes through here.
    void clamp_caret();

    // Records new_text, keeping the caret valid, and emits "text_changed" only when
    // the value changed. The caller moves the caret first.
    void commit_text(std::string new_text);

    // Edit primitives; each returns whether it changed the text.
    bool insert_codepoint(char32_t codepoint);
    bool backspace();
    bool delete_forward();

public:
    explicit LineEdit(const std::string& node_name = "Unnamed LineEdit",
                      Vec2 node_position = {0, 0});

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // One text line plus the frame; never sized to the content, so it does not
    // wobble as you type.
    Vec2 get_minimum_size() const override;

    // --- Text ---------------------------------------------------------------

    void set_text(const std::string& text);
    const std::string& get_text() const { return text; }

    // Empties the field, caret at the start; emits "text_changed" when there was text.
    void clear();

    // --- Placeholder --------------------------------------------------------

    // Shown in placeholder_color while the field is empty; never part of the text.
    void set_placeholder(const std::string& text) { placeholder = text; }
    const std::string& get_placeholder() const { return placeholder; }

    // --- Editing ------------------------------------------------------------

    // Longest text in codepoints; 0 or less is unlimited. Stops inserts, does not
    // truncate existing text.
    void set_max_length(int length) { max_length = length; }
    int get_max_length() const { return max_length; }

    // Byte offset into the UTF-8 text. set_caret snaps an index inside a codepoint
    // back to its lead byte.
    std::size_t get_caret() const { return caret; }
    void set_caret(std::size_t index);

    // --- Echo ---------------------------------------------------------------

    // Draws one secret_character per visible codepoint; the stored text is unchanged.
    void set_secret(bool enabled) { secret = enabled; }
    bool is_secret() const { return secret; }

    void set_secret_character(char32_t character) { secret_character = character; }

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    void set_stylebox(StyleBox box);

    void set_text_color(uint16_t color) { text_color = color; }
    void set_text_color(const std::string& color_name);

    void set_placeholder_color(uint16_t color) { placeholder_color = color; }
    void set_placeholder_color(const std::string& color_name);

    // The band the text and caret sit on. A StyleBox background wins where the two overlap.
    void set_background_color(uint16_t color) { background_color = color; }
    void set_background_color(const std::string& color_name);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LINEEDIT_HPP
