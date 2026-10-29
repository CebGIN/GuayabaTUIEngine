#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

#include <guayaba/Nodes/UI/SpinBox.hpp>
#include <guayaba/Resources/Theme.hpp>

namespace guayaba {

namespace {

// Same floor the public setter documents. Kept here so the constant has a name.
constexpr int MAX_DECIMALS = 15;

} // namespace

SpinBox::SpinBox(const std::string& node_name, Vec2 node_position)
    : LineEdit(node_name, node_position) {
    declare_signal<double>("value_changed");

    // Reserve the two right columns for the arrows. Because the text area is the
    // StyleBox content rect, the margin is what keeps the text from running under
    // them, and LineEdit::get_minimum_size() counts the margin already.
    const EdgeSize margin = stylebox.get_content_margin();
    stylebox.set_content_margin(margin.left, margin.top, margin.right + 2, margin.bottom);

    reformat();

    connect_signal<std::string>("text_changed", [this](const std::string& new_text) {
        if (formatting) return;

        double parsed = 0.0;
        if (parse_number(new_text, parsed)) set_value_live(parsed);
    });

    // Losing the focus commits what was typed, like Enter does.
    connect_signal<>("focus_exited", [this] { reformat(); });
}

double SpinBox::clamp_value(double new_value) const {
    if (max_value <= min_value) return min_value;
    return std::clamp(new_value, min_value, max_value);
}

double SpinBox::snap(double new_value) const {
    const double factor = std::pow(10.0, static_cast<double>(decimals));
    return std::round(new_value * factor) / factor;
}

std::string SpinBox::format_value(double new_value) const {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(decimals) << new_value;
    return stream.str();
}

bool SpinBox::parse_number(const std::string& input, double& out) {
    if (input.empty()) return false;

    // istringstream with the classic locale is the portable way to get strtod's
    // behaviour without inheriting the process locale's decimal separator.
    std::istringstream stream(input);
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> out;

    return !stream.fail() && stream.eof() && std::isfinite(out);
}

void SpinBox::reformat() {
    formatting = true;
    set_text(format_value(value));
    set_caret(get_text().size());
    formatting = false;
}

void SpinBox::set_value_live(double new_value) {
    const double effective = snap(clamp_value(new_value));
    if (effective == value) return;

    value = effective;
    emit_signal("value_changed", value);
}

void SpinBox::set_value(double new_value) {
    const double effective = snap(clamp_value(new_value));
    const bool changed = effective != value;

    value = effective;
    reformat();

    if (changed) emit_signal("value_changed", value);
}

void SpinBox::set_range(double min, double max) {
    min_value = min;
    max_value = max;

    const double effective = snap(clamp_value(value));
    const bool changed = effective != value;

    value = effective;
    reformat();

    if (changed) emit_signal("value_changed", value);
}

void SpinBox::set_step(double new_step) {
    step = new_step > 0.0 ? new_step : 1e-9;
}

void SpinBox::set_decimals(int new_decimals) {
    decimals = std::clamp(new_decimals, 0, MAX_DECIMALS);

    const double snapped = snap(value);
    const bool changed = snapped != value;

    value = snapped;
    reformat();

    if (changed) emit_signal("value_changed", value);
}

Rect SpinBox::arrow_zone() const {
    const Rect content = content_rect();
    if (content.is_empty()) return Rect{};

    return Rect{
        Vec2{content.pos.x + content.size.x, content.pos.y},
        Vec2{2, content.size.y},
    };
}

bool SpinBox::step_by(double delta) {
    if (is_read_only()) return false;

    double next = value + delta;

    if (wrap) {
        if (next > max_value) next = min_value;
        else if (next < min_value) next = max_value;
    }

    const double effective = snap(clamp_value(next));
    if (effective == value) return false;

    value = effective;
    reformat();
    emit_signal("value_changed", value);
    return true;
}

bool SpinBox::on_gui_input(const InputEvent& event) {
    if (is_read_only()) {
        // Tab and Escape stay available to the scene; editing keys do nothing.
        if (event.type == InputEvent::Type::KEY && event.pressed) {
            if (event.key == Key::TAB || event.key == Key::ESCAPE) {
                return LineEdit::on_gui_input(event);
            }
            return true;
        }

        if (event.type == InputEvent::Type::MOUSE_BUTTON && event.pressed &&
            event.mouse_button == MouseButton::LEFT && is_inside(event.mouse_pos)) {
            grab_focus();
            return true;
        }

        return LineEdit::on_gui_input(event);
    }

    if (event.type == InputEvent::Type::KEY && event.pressed) {
        switch (event.key) {
            case Key::UP:
                step_by(step);
                return true;
            case Key::DOWN:
                step_by(-step);
                return true;
            case Key::PAGE_UP:
                step_by(10.0 * step);
                return true;
            case Key::PAGE_DOWN:
                step_by(-10.0 * step);
                return true;
            case Key::ENTER:
                // Let the field announce text_submitted with what was typed, then
                // clamp and rewrite it.
                LineEdit::on_gui_input(event);
                reformat();
                return true;
            default:
                break;
        }
    }

    if (event.type == InputEvent::Type::MOUSE_BUTTON && event.pressed &&
        event.mouse_button == MouseButton::LEFT) {
        const Rect zone = arrow_zone();
        if (!zone.is_empty() && zone.contains(event.mouse_pos)) {
            grab_focus();

            // Top half increments, bottom half decrements.
            const int split = zone.pos.y + (zone.size.y + 1) / 2;
            step_by(event.mouse_pos.y < split ? step : -step);
            return true;
        }
    }

    return LineEdit::on_gui_input(event);
}

void SpinBox::on_draw(ConsoleRenderer& renderer, const Rect& clip) {
    LineEdit::on_draw(renderer, clip);

    const Rect zone = arrow_zone();
    if (zone.is_empty()) return;

    renderer.put_char({zone.pos.x, zone.pos.y}, L'▲', text_color, background_color, clip);
    if (zone.size.y > 1) {
        renderer.put_char({zone.pos.x, zone.pos.y + zone.size.y - 1},
                          L'▼', text_color, background_color, clip);
    }
}

void SpinBox::apply_theme(const Theme& theme) {
    // spin_box wins over line_edit, and the two arrow columns are added on top of
    // whatever box the theme provides.
    StyleBox box = theme.get_style("spin_box", theme.get_style("line_edit", stylebox));
    const EdgeSize margin = box.get_content_margin();
    box.set_content_margin(margin.left, margin.top, margin.right + 2, margin.bottom);
    stylebox = std::move(box);

    text_color = theme.get_color("text.primary", text_color);
    placeholder_color = theme.get_color("text.secondary", placeholder_color);
}

} // namespace guayaba
