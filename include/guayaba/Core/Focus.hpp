#ifndef GUAYABA_CORE_FOCUS_HPP
#define GUAYABA_CORE_FOCUS_HPP

#include <cstdint>
#include <vector>

namespace guayaba {

class Control;
class Node;

/*
    Which control has the keyboard, for the whole process. Holds the control's id
    rather than a pointer, so a freed control loses the focus.

        field->grab_focus();                        // emits focus_exited / focus_entered
        FocusManager::get_instance().focus_next();  // Tab

    Tab order is tree order, wrapping from the last focusable control to the first.
*/
class FocusManager {
public:
    static FocusManager& get_instance();

    FocusManager(const FocusManager&) = delete;
    FocusManager& operator=(const FocusManager&) = delete;

    // The focused control, or nullptr once it is freed.
    Control* get_focused();

    // True when `control` holds the focus. False for nullptr.
    bool is_focused(const Control* control) const;

    // The focusable controls under `root`, in tree order. This is what Tab walks.
    std::vector<Control*> get_focusable(Node* root);

    // Moves focus to the next/previous focusable control in the current scene, wrapping.
    void focus_next();
    void focus_prev();

    // Drops the focus, emitting focus_exited on whoever held it.
    void clear();

private:
    FocusManager() = default;

    // Moves the focus, emitting focus_exited on the previous holder and focus_entered on the new one.
    void set_focused(Control* control);

    uint64_t focused_id_ = 0;

    // Control calls set_focused only; it reads the flag it owns for is_focused().
    friend class Control;
};

} // namespace guayaba

#endif // GUAYABA_CORE_FOCUS_HPP
