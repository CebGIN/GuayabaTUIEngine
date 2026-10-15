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

    It is the field half of what a Button is the press half: Label's text inside a
    box, plus the caret and the key handling that turns it into something the user
    can write into.

        +--------------------+
        | user_name_|        |
        +--------------------+

    HOW FOCUS AND INPUT GET HERE
    Focus is process-wide (FocusManager) and lives nowhere else, so the field only
    says what it wants - FocusMode::CLICK - and takes the focus itself when the
    left button lands on it. The engine deliberately does not auto-focus on click,
    because a click can also mean "select" or "start a drag"; a field is the one
    control where "click focuses" is the whole behaviour, so it does the
    grab_focus() and consumes the press.

    Once focused, KEY events reach on_gui_input and nothing else does; Control
    gates them by is_focused(), so there is no focus test here. TAB and ESCAPE are
    declined rather than consumed, so the scene keeps them for focusing the next
    field and for cancelling.

    THE CARET IS A BYTE OFFSET
    text is UTF-8 and the caret is an index into it in bytes, not in characters.
    That keeps the public API a plain std::size_t and lets an insert be a
    substring splice. The cost is that the caret has to be kept on a codepoint
    boundary - snapping a mid-codepoint index backwards to its lead byte - and that
    display columns are counted by decoding, not by subtracting offsets. All of
    that is private; get_caret()/set_caret() speak bytes and stay valid.

    max_length counts CODEPOINTS, not bytes, because that is the count a user sees:
    "10 characters" should not mean "10 bytes" for an accented word. It stops
    inserts once the field is full; it does not truncate text set through set_text,
    which is a deliberate caller override of the field's own limit.

    SECRET MODE
    set_secret(true) draws one secret_character per visible codepoint, so the
    width on screen still matches what was typed without leaking the content. The
    stored text is untouched - get_text() still returns what the user entered -
    because a password field still has to hand the password to whoever submits it.

    SIGNALS
    "text_changed" (std::string) fires whenever the stored text actually changes -
    typing, backspace, delete, clear, set_text - and only then, so a listener can
    treat it as "the value is different now" rather than "a key was pressed".
    "text_submitted" (std::string) fires on ENTER. Both carry the current text, so a
    single connected callback can read the field without asking for it back.
*/
class LineEdit : public Control {
protected:
    StyleBox stylebox;

    std::string text;
    std::string placeholder;

    // Byte offset into text, always on a codepoint boundary and never past size().
    std::size_t caret = 0;

    // 0 or less means "no limit". Counted in codepoints - see the class comment.
    int max_length = 0;

    bool secret = false;
    char32_t secret_character = U'*';

    uint16_t text_color = 7;             // White, the TermChar default
    uint16_t placeholder_color = 8;      // BRIGHT_BLACK, so the hint reads as a hint
    uint16_t background_color = 0;       // Black, the TermChar default

    // Where the text goes: the box's rect minus its border and margins. Empty when
    // the box is too small to hold anything, which the draw treats as "draw
    // nothing" rather than as something to compensate for.
    Rect content_rect() const;

    // The interaction: a left press takes the focus, and every editing key is
    // handled here while the field holds it. Declines TAB and ESCAPE so the scene
    // still sees them.
    bool on_gui_input(const InputEvent& event) override;

    // Moves the caret back to the lead byte of its codepoint and inside the text.
    // Every path that changes text or caret funnels through here, which is what
    // makes "always valid" a property and not a promise.
    void clamp_caret();

    // Records new_text, keeping the caret valid, and emits "text_changed" only when
    // the value really moved. The caller is responsible for moving the caret to
    // where the edit left it before calling in.
    void commit_text(std::string new_text);

    // The edit primitives, each returning whether it changed the text. They exist
    // so on_gui_input is a key dispatch and not a string-editing routine.
    bool insert_codepoint(char32_t codepoint);
    bool backspace();
    bool delete_forward();

public:
    explicit LineEdit(const std::string& node_name = "Unnamed LineEdit",
                      Vec2 node_position = {0, 0});

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // One text line plus the frame; wide enough for the caller's floor plus the
    // frame, but never sized to the content - a field is as wide as the caller
    // makes it and scrolls its text, so its minimum does not wobble as you type.
    Vec2 get_minimum_size() const override;

    // --- Text ---------------------------------------------------------------

    void set_text(const std::string& text);
    const std::string& get_text() const { return text; }

    // Empties the field and leaves the caret at the start. Goes through the same
    // change path as an edit, so it emits "text_changed" when there was text.
    void clear();

    // --- Placeholder --------------------------------------------------------

    // Shown in placeholder_color while the field is empty. It is a hint and never
    // becomes part of the text.
    void set_placeholder(const std::string& text) { placeholder = text; }
    const std::string& get_placeholder() const { return placeholder; }

    // --- Editing ------------------------------------------------------------

    // The longest the text may get, in codepoints. 0 or less is unlimited.
    // Existing text is not truncated: a limit stops inserts, it does not undo
    // what set_text already put there.
    void set_max_length(int length) { max_length = length; }
    int get_max_length() const { return max_length; }

    // Byte offset into the UTF-8 text. set_caret snaps an index that falls inside
    // a codepoint back to its lead byte, so the value read back is always one the
    // next insert or delete can splice at.
    std::size_t get_caret() const { return caret; }
    void set_caret(std::size_t index);

    // --- Echo ---------------------------------------------------------------

    // Draws one secret_character per visible codepoint. The stored text is
    // unchanged; only the rendering is masked.
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

    // The band the text and the caret sit on. It is the field's own background,
    // the same way Label carries one; a StyleBox background wins over it where
    // the two overlap, so a framed field can still paint its own surface.
    void set_background_color(uint16_t color) { background_color = color; }
    void set_background_color(const std::string& color_name);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LINEEDIT_HPP
