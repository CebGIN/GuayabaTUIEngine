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
    window.

        +--------------------+
        | thread 1           |
        | thread 2           |  <- cursor band
        | thread 3           |
        +--------------------+

    It is the third of the three generic verbs a terminal UI keeps needing: a
    Button presses, a LineEdit writes, a ListView chooses. Nothing here is
    specific to threads or menus - the items are plain strings and the caller tints
    or reacts to them.

    VIRTUAL CONTENTS
    Every item is stored, but only the rows that fit in the content rect are drawn.
    The window is the pair (scroll, visible_count): the cursor can be anywhere in
    the list while the viewport slides. That is what makes a log or a process list
    with thousands of rows cost the same to draw as one with ten.

    Selection is an index, and -1 means "nothing selected" so an empty or freshly
    built list is honest about it. set_selected and the movement helpers clamp into
    the item range; the window is then nudged just far enough to keep the cursor
    on screen, never centred, because a list that jumps is harder to read than one
    that steps.

    SIGNALS
    "selection_changed" (int) fires whenever the cursor actually moves, whatever
    moved it - a key, a click, a programmatic set_selected. "item_activated" (int)
    fires on ENTER or a second click, which is the "open this one" gesture a menu
    and a log both want. Both carry the row index so a single reactor can look it
    up without asking for it back.

    A press takes the focus the way a LineEdit's does - the engine does not
    auto-focus - and selects the row under the pointer. The matching release is
    consumed too, so a click that starts here cannot leak to a control behind.
*/
class ListView : public Control {
public:
    explicit ListView(const std::string& node_name = "Unnamed ListView",
                      Vec2 node_position = {0, 0});

    void draw(ConsoleRenderer& renderer, const Rect& clip) override;

    // The caller's floor plus the frame, at least one row tall - the same shape as
    // LineEdit's rule. The list is as tall as it is told and scrolls its items;
    // the minimum never grows to fit them.
    Vec2 get_minimum_size() const override;

    // Public here, unlike the other controls: the spec lists it as part of the
    // ListView surface, and the frame route calls it through Control anyway.
    bool on_gui_input(const InputEvent& event) override;

    // --- Items --------------------------------------------------------------

    void set_items(std::vector<std::string> new_items);
    const std::vector<std::string>& get_items() const { return items; }
    std::size_t item_count() const { return items.size(); }

    // --- Selection ----------------------------------------------------------

    // -1 when nothing is selected.
    int selected() const { return selected_index; }

    // Clamped to the item range; a negative index means "clear the selection".
    // Emits "selection_changed" when the cursor actually moves.
    void set_selected(int index);

    void select_first();
    void select_last();

    // Moves the cursor by delta rows, clamping at the ends. From "nothing
    // selected" a positive delta lands on the first row and a negative one on the
    // last, so the first keypress in either direction always selects something.
    void move_selection(int delta);

    // --- Scroll window ------------------------------------------------------

    int scroll() const { return scroll_offset; }

    // Offset of the top visible row, clamped to [0, item_count - visible_count].
    void set_scroll(int offset);

    // Rows that fit in the content rect. Zero before the first layout pass or on a
    // collapsed rect.
    int visible_count() const;

    // --- Appearance ---------------------------------------------------------

    StyleBox& get_stylebox() { return stylebox; }
    const StyleBox& get_stylebox() const { return stylebox; }

    void set_stylebox(StyleBox box);

    void set_text_color(uint16_t color) { text_color = color; }
    void set_text_color(const std::string& color_name);

    // The cursor band: a background the width of the content rect and the text
    // colour that reads on top of it. BRIGHT_BLACK is the conventional terminal
    // selection, dark enough to sit under the row's own colour.
    void set_selection_background_color(uint16_t color) { selection_background_color = color; }
    void set_selection_background_color(const std::string& color_name);

    void set_selection_text_color(uint16_t color) { selection_text_color = color; }
    void set_selection_text_color(const std::string& color_name);

    // Tints one row's text - a zombie in red, a stopped thread in yellow. The
    // selection colours still win while that row is the cursor, so the band stays
    // legible.
    void set_item_color(int index, uint16_t color);

    void clear_item_colors();

protected:
    // The area inside the border and margins. Empty when the box is too small to
    // hold anything, which the draw reads as "draw nothing".
    Rect content_rect() const;

private:
    StyleBox stylebox;

    std::vector<std::string> items;

    int selected_index = -1;
    int scroll_offset = 0;

    uint16_t text_color = 7;                   // White, the TermChar default
    uint16_t selection_background_color = 8;   // BRIGHT_BLACK
    uint16_t selection_text_color = 15;        // BRIGHT_WHITE

    // Sparse on purpose: most lists tint a handful of rows, not all of them.
    std::map<int, uint16_t> item_colors;

    // Moves the cursor there, scrolling to keep it visible, and emits only when it
    // really moved. Every selection path funnels through here, so "changed" means
    // changed whatever asked.
    void apply_selection(int index);

    // Keeps the cursor's row inside the window by moving the window the minimum
    // amount.
    void ensure_cursor_visible();

    // Keeps scroll_offset inside [0, item_count - visible]. Called after anything
    // that can change the item count, the window height or the offset.
    void clamp_scroll();

    // Paints one row of the window: the full content width, so the band and the
    // clearing of the row both span it.
    void draw_row(ConsoleRenderer& renderer, const Rect& content, int row,
                  const std::string& item, uint16_t fg, uint16_t bg, const Rect& clip);
};

} // namespace guayaba

#endif // GUAYABA_NODES_UI_LISTVIEW_HPP
