#include <guayaba/Core/Focus.hpp>

#include <guayaba/Core/NodeRef.hpp>
#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Nodes/UI/Control.hpp>
#include <guayaba/Scene/SceneManager.hpp>

namespace guayaba {

namespace {

void collect_focusable(Node* node, std::vector<Control*>& out) {
    // A hidden subtree is skipped whole: neither it nor its descendants can take
    // the focus, so there is no point walking into it.
    if (node == nullptr || !node->is_visible_in_tree()) return;

    if (Control* control = dynamic_cast<Control*>(node)) {
        if (control->get_focus_mode() != Control::FocusMode::NONE) {
            out.push_back(control);
        }
    }
    const size_t count = node->get_child_count();
    for (size_t i = 0; i < count; ++i) {
        collect_focusable(node->get_child<Node>(i).get(), out);
    }
}

} // namespace

FocusManager& FocusManager::get_instance() {
    static FocusManager instance;
    return instance;
}

Control* FocusManager::get_focused() {
    if (focused_id_ == 0) return nullptr;
    return dynamic_cast<Control*>(Node::resolve_instance_id(focused_id_));
}

bool FocusManager::is_focused(const Control* control) const {
    return control != nullptr && focused_id_ != 0 &&
           control->instance_id() == focused_id_;
}

std::vector<Control*> FocusManager::get_focusable(Node* root) {
    std::vector<Control*> out;
    collect_focusable(root, out);
    return out;
}

void FocusManager::set_focused(Control* control) {
    Control* old = get_focused();
    if (old == control) return;

    if (old != nullptr) old->set_focused_internal(false);

    focused_id_ = (control != nullptr) ? control->instance_id() : 0;

    if (control != nullptr) control->set_focused_internal(true);
}

void FocusManager::focus_next() {
    Control* current = get_focused();
    std::vector<Control*> order =
        get_focusable(SceneManager::get_instance().get_current_scene().get());
    if (order.empty()) return;

    size_t index = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        if (order[i] == current) {
            index = i;
            break;
        }
    }
    const size_t next = (current != nullptr) ? (index + 1) % order.size() : 0;
    set_focused(order[next]);
}

void FocusManager::focus_prev() {
    Control* current = get_focused();
    std::vector<Control*> order =
        get_focusable(SceneManager::get_instance().get_current_scene().get());
    if (order.empty()) return;

    size_t index = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        if (order[i] == current) {
            index = i;
            break;
        }
    }
    const size_t prev = (current != nullptr) ? (index + order.size() - 1) % order.size()
                                             : order.size() - 1;
    set_focused(order[prev]);
}

void FocusManager::clear() {
    set_focused(nullptr);
}

} // namespace guayaba
