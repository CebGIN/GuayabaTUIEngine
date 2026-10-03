#ifndef GUAYABA_TERMINAL_IOSTERMINAL_HPP
#define GUAYABA_TERMINAL_IOSTERMINAL_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Core/TermChar.hpp>
#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

class IOSTerminal {
public:
    virtual ~IOSTerminal() = default;

    // Lifecycle
    virtual void init() = 0;
    virtual void restore() = 0;

    // Output
    virtual void set_cursor_visible(bool visible) = 0;
    virtual void set_cursor_position(Vec2 pos) = 0;
    virtual void present_buffer(const std::vector<TermChar>& buffer, Vec2 buffer_size) = 0;

    // Dimensions of the physical terminal, queried fresh each call so that
    // resizes are picked up. Implementations fall back to the current buffer
    // size if the query fails.
    virtual Vec2 get_screen_size(Vec2 fallback) const = 0;

    // Input
    virtual void poll_events() = 0;
    virtual Vec2 get_mouse_position() const = 0;
    virtual bool is_left_click_pressed() const = 0;
    virtual bool is_right_click_pressed() const = 0;
    virtual bool is_left_click_just_pressed() const = 0;
    virtual bool is_right_click_just_pressed() const = 0;

    // Blocking text input
    virtual std::string get_line_input(Vec2 position) = 0;

    template<typename T>
    T get_typed_input(Vec2 position) {
        T input;
        set_cursor_position(position);
        set_cursor_visible(true);
        std::cin >> input;
        set_cursor_visible(false);
        return input;
    }
};

// Factory function — returns the backend for the platform being compiled.
std::unique_ptr<IOSTerminal> create_system_terminal();

} // namespace guayaba

#endif // GUAYABA_TERMINAL_IOSTERMINAL_HPP