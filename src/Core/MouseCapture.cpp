#include "MouseCapture.hpp"

#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/UI/Control.hpp>

namespace guayaba::detail {

MouseCapture& MouseCapture::instance() {
    // Function-local static rather than a global: one instance across
    // translation units, constructed on first use.
    static MouseCapture state;
    return state;
}

Control* MouseCapture::get_captured() const {
    if (captured_id_ == 0) return nullptr;

    Control* control = dynamic_cast<Control*>(Node::resolve_instance_id(captured_id_));
    if (control == nullptr) {
        // The node died without releasing; forget the id instead of resolving it
        // again on every event.
        captured_id_ = 0;
    }
    return control;
}

void MouseCapture::capture(Control* control) {
    captured_id_ = (control != nullptr) ? control->instance_id() : 0;
}

void MouseCapture::release(const Control* control) {
    if (control != nullptr && control->instance_id() == captured_id_) {
        captured_id_ = 0;
    }
}

void MouseCapture::clear() {
    captured_id_ = 0;
}

} // namespace guayaba::detail
