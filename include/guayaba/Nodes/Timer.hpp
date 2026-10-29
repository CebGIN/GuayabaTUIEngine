#ifndef GUAYABA_NODES_TIMER_HPP
#define GUAYABA_NODES_TIMER_HPP

#include <string>

#include <guayaba/Nodes/Node.hpp>

namespace guayaba {

/*
    A one-shot or repeating countdown that emits timeout() when a period elapses.

    A large delta may span several periods; the signal fires once per period. In
    one-shot mode the timer stops after the first, otherwise it keeps going and
    get_ticks() counts every emission.

    start() restarts the current period but leaves get_ticks() alone, so a restarted
    timer still reports the emissions it has made. A hidden timer does not advance:
    the visibility check lives in Node::update, before on_update.
*/
class Timer : public Node {
private:
    static constexpr double MINIMUM_WAIT_TIME = 0.001;

    double wait_time = 1.0;
    double accumulator = 0.0;

    bool running = false;
    bool one_shot = false;
    bool autostart = false;

    int ticks = 0;

public:
    explicit Timer(const std::string& node_name = "Unnamed Timer");

    void on_update(double delta_time) override;

    // --- Period -------------------------------------------------------------

    // Zero or less normalizes to 0.001 s.
    void set_wait_time(double seconds);
    double get_wait_time() const { return wait_time; }

    void set_one_shot(bool value) { one_shot = value; }
    bool is_one_shot() const { return one_shot; }

    void set_autostart(bool value) { autostart = value; }
    bool get_autostart() const { return autostart; }

    // --- Run state ----------------------------------------------------------

    // Restarts the current period from zero; ticks are kept.
    void start();

    // Sets the period and starts.
    void start(double wait_seconds);

    void stop() { running = false; }
    bool is_stopped() const { return !running; }

    // Remaining time in the current period. Frozen while stopped: it is what the
    // timer would have left if it were resumed.
    double get_time_left() const;

    // Emissions of timeout() since construction. Not reset by start().
    int get_ticks() const { return ticks; }

protected:
    // Starts the timer when autostart is on.
    void enter_tree() override;
};

} // namespace guayaba

#endif // GUAYABA_NODES_TIMER_HPP
