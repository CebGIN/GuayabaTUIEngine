#ifndef GUAYABA_NODES_UI_CHECKBOX_HPP
#define GUAYABA_NODES_UI_CHECKBOX_HPP

#include <cstdint>
#include <string>
#include <utility>

#include <guayaba/Core/InputEvent.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A labelled on/off switch: `[x] text` or `[ ] text`.

    The indicator is three columns wide. When there is text it is separated by one
    space, so the minimum is 3 cells without text and 4 + the text's width with it.
    A left click or Space/Enter while focused toggles the state; toggled(bool) is
    emitted only when it actually changes.
*/
class CheckBox : public Control {
private:
    StyleBox stylebox;

    std::string text;
    bool checked = false;

    char32_t checked_mark = U'x';
    char32_t unchecked_mark = U' ';

    uint16_t text_color = 7;       // White, the TermChar default
    uint16_t background_color = 0; // Black, the TermChar default

    // The rect inside the StyleBox frame, where the indicator is drawn.
    Rect content_rect() const;

public:
    explicit CheckBox(const std::string& node_name = "Unnamed CheckBox",
                      Vec2 node_position = {0, 0});

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    Vec2 get_minimum_size() const override;

    // --- Text ---------------------------------------------------------------

    void set_text(std::string new_text) { text = std::move(new_text); }
    const std::string& get_text() const { return text; }

    // --- State --------------------------------------------------------------

    // Emits toggled(bool) only when the state changes.
    void set_checked(bool value);
    bool is_checked() const { return checked; }
    void toggle() { set_checked(!checked); }

    // Both glyphs must be printable single cells. Throws std::invalid_argument
    // otherwise.
    void set_glyphs(char32_t checked_glyph, char32_t unchecked_glyph);

    // --- Appearance ---------------------------------------------------------

    void set_text_color(uint16_t color) { text_color = color; }
    void set_background_color(uint16_t color) { background_color = color; }

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

protected:
    bool on_gui_input(const InputEvent& event) override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_CHECKBOX_HPP
