#include <algorithm>
#include <cmath>

#include <guayaba/Nodes/Timer.hpp>

namespace guayaba {

namespace {

// A pathological delta against a tiny wait_time must not leave an endless
// backlog for the next frame, nor spin here for an unbounded number of ticks.
constexpr int MAX_TICKS_PER_FRAME = 1000;

} // namespace

Timer::Timer(const std::string& node_name) : Node(node_name) {
    declare_signal("timeout");
}

void Timer::set_wait_time(double seconds) {
    wait_time = seconds > 0.0 ? seconds : MINIMUM_WAIT_TIME;
}

void Timer::start() {
    accumulator = 0.0;
    running = true;
}

void Timer::start(double wait_seconds) {
    set_wait_time(wait_seconds);
    start();
}

double Timer::get_time_left() const {
    return std::max(0.0, wait_time - accumulator);
}

void Timer::on_update(double delta_time) {
    if (running && delta_time > 0.0) {
        accumulator += delta_time;

        int fired = 0;
        while (running && accumulator >= wait_time && fired < MAX_TICKS_PER_FRAME) {
            accumulator -= wait_time;
            ++ticks;
            ++fired;

            // A reactor may stop or restart the timer; the loop condition reads
            // the flag again.
            emit_signal("timeout");

            if (one_shot) running = false;
        }

        if (fired == MAX_TICKS_PER_FRAME) {
            accumulator = std::fmod(accumulator, wait_time);
        }
    }

    Node::on_update(delta_time);
}

void Timer::enter_tree() {
    Node::enter_tree();

    if (autostart) start();
}

} // namespace guayaba
