#ifndef GUAYABA_SRC_CORE_MOUSECAPTURE_HPP
#define GUAYABA_SRC_CORE_MOUSECAPTURE_HPP

#include <cstdint>

namespace guayaba {

class Control;

namespace detail {

/*
    Which control has the mouse, for the whole process. Holds the control's id
    rather than a pointer, so a freed control loses the capture instead of
    leaving a dangling pointer behind.

    Internal header: not installed, not part of the public API. Control exposes
    the operations; the authority lives here, mirroring FocusManager.
*/
class MouseCapture {
public:
    static MouseCapture& instance();

    MouseCapture(const MouseCapture&) = delete;
    MouseCapture& operator=(const MouseCapture&) = delete;

    // The captor, or nullptr once it is freed. A dead id is dropped on the way
    // out, so callers never see a stale capture.
    Control* get_captured() const;

    // Takes the capture for `control`, replacing any previous holder.
    void capture(Control* control);

    // Releases the capture only when `control` holds it.
    void release(const Control* control);

    // Releases unconditionally, e.g. on a scene change.
    void clear();

private:
    MouseCapture() = default;

    // Mutable because get_captured() is a const query that may drop a dead id.
    mutable uint64_t captured_id_ = 0;
};

} // namespace detail
} // namespace guayaba

#endif // GUAYABA_SRC_CORE_MOUSECAPTURE_HPP
