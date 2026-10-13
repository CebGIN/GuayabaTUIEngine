#ifndef GUAYABA_CORE_FOCUS_HPP
#define GUAYABA_CORE_FOCUS_HPP

#include <cstdint>
#include <vector>

namespace guayaba {

class Control;
class Node;

/*
    Which control has the keyboard, for the whole process.

    A singleton, like SignalBus and for the same reason: focus is a question
    about the entire tree at once, so no one node can answer it. It is not a
    stack, not per-window, and not remembered across a scene change; a control
    that is freed loses it, which is why the id is held rather than the pointer.

        field->grab_focus();            // emits focus_exited / focus_entered
        FocusManager::get_instance().focus_next();   // Tab

    TAB ORDER IS TREE ORDER, which makes it the order things are built in and
    wraps from the last focusable control back to the first.
*/
class FocusManager {
public:
    static FocusManager& get_instance();

    FocusManager(const FocusManager&) = delete;
    FocusManager& operator=(const FocusManager&) = delete;

    // The focused control, or nullptr. Resolved through the id, so it is nullptr
    // once that control is freed rather than dangling.
    Control* get_focused();

    // True when `control` holds the focus. False for nullptr.
    bool is_focused(const Control* control) const;

    // The focusable controls under `root`, in tree order. This is what Tab walks.
    std::vector<Control*> get_focusable(Node* root);

    // Moves focus to the next/previous focusable control in tree order, wrapping.
    // Does nothing when there are none. Uses the current scene as the root.
    void focus_next();
    void focus_prev();

    // Drops the focus, emitting focus_exited on whoever held it.
    void clear();

private:
    FocusManager() = default;

    // Moves the focus, emitting focus_exited on the old holder and focus_entered
    // on the new one. Called by Control::grab_focus / release_focus and the Tab
    // helpers. Not the route a scene takes - a scene calls grab_focus.
    void set_focused(Control* control);

    uint64_t focused_id_ = 0;

    // Control calls set_focused only; it reads the flag it owns for is_focused().
    friend class Control;
};

} // namespace guayaba

#endif // GUAYABA_CORE_FOCUS_HPP
