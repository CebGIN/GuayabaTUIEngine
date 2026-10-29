#ifndef GUAYABA_NODES_UI_LISTVIEW_HPP
#define GUAYABA_NODES_UI_LISTVIEW_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <guayaba/Core/Rect.hpp>
#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>
#include <guayaba/Resources/Stylebox.hpp>

namespace guayaba {

/*
    A Control that shows a vertical list of text rows with a cursor and a scroll
    window. Items are plain strings; the caller tints or reacts to them.

        +--------------------+
        | thread 1           |
        | thread 2           |  <- cursor band
        | thread 3           |
        +--------------------+

    Every item is stored, but only the rows that fit the content rect are drawn.
    The window is the pair (scroll, visible_count), so a list of thousands of rows
    draws the same as one of ten.

    Selection is an index, -1 meaning nothing selected. set_selected and the
    movement helpers clamp into the item range and nudge the window just enough to
    keep the cursor visible.

    Signals: "selection_changed" (int) when the cursor moves, "item_activated"
    (int) on ENTER or a second click. Both carry the row index.

    A press takes focus like a LineEdit's does, and selects the row under the
    pointer; the matching release is consumed too.
*/
class ListView : public Control {
public:
    explicit ListView(const std::string& node_name = "Unnamed ListView",
                      Vec2 node_position = {0, 0});

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // The caller's floor plus the frame, at least one row tall. The list scrolls
    // its items rather than growing to fit them.
    Vec2 get_minimum_size() const override;

    // Public, unlike the other controls, because the ListView API exposes it.
    bool on_gui_input(const InputEvent& event) override;

    // --- Items --------------------------------------------------------------

    void set_items(std::vector<std::string> new_items);
    const std::vector<std::string>& get_items() const { return items; }
    std::size_t item_count() const { return items.size(); }

    // --- Selection ----------------------------------------------------------

    // -1 when nothing is selected.
    int selected() const { return selected_index; }

    // Clamped to the item range; negative clears the selection. Emits
    // "selection_changed" when the cursor moves.
    void set_selected(int index);

    void select_first();
    void select_last();

    // Moves the cursor by delta rows, clamping at the ends. From no selection, a
    // positive delta lands on the first row and a negative one on the last.
    void move_selection(int delta);

    // --- Scroll window ------------------------------------------------------

    int scroll() const { return scroll_offset; }

    // Offset of the top visible row, clamped to [0, item_count - visible_count].
    void set_scroll(int offset);

    // Rows that fit the content rect; zero before the first layout pass.
    int visible_count() const;

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    void set_stylebox(StyleBox box);

    void set_text_color(uint16_t color) { text_color = color; }
    void set_text_color(const std::string& color_name);

    // The cursor band: background and text colour. BRIGHT_BLACK is the
    // conventional terminal selection.
    void set_selection_background_color(uint16_t color) { selection_background_color = color; }
    void set_selection_background_color(const std::string& color_name);

    void set_selection_text_color(uint16_t color) { selection_text_color = color; }
    void set_selection_text_color(const std::string& color_name);

    // Tints one row's text. The selection colours still win while that row is the
    // cursor.
    void set_item_color(int index, uint16_t color);

    void clear_item_colors();

protected:
    // The area inside the border and margins; empty when the box is too small.
    Rect content_rect() const;

private:
    StyleBox stylebox;

    std::vector<std::string> items;

    int selected_index = -1;
    int scroll_offset = 0;

    uint16_t text_color = 7;                   // White, the TermChar default
    uint16_t selection_background_color = 8;   // BRIGHT_BLACK
    uint16_t selection_text_color = 15;        // BRIGHT_WHITE

    // Sparse: most lists tint only a few rows.
    std::map<int, uint16_t> item_colors;

    // Moves the cursor there, scrolling to keep it visible, and emits only when it
    // moved.
    void apply_selection(int index);

    // Moves the window the minimum amount so the cursor's row is visible.
    void ensure_cursor_visible();

    // Keeps scroll_offset inside [0, item_count - visible].
    void clamp_scroll();

    // Paints one row across the full content width.
    void draw_row(ConsoleRenderer& renderer, const Rect& content, int row,
                  const std::string& item, uint16_t fg, uint16_t bg, const Rect& clip);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LISTVIEW_HPP
