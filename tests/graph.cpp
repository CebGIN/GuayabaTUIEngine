// Headless checks for the Graph control: the value buffer, the capacity ring, the
// automatic vs fixed range, the level mapping and a draw smoke pass. No terminal;
// the mapping is reached through a subclass because the renderer exposes no buffer
// to read glyphs back from.

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

constexpr Vec2 SCREEN{80, 24};

// Reaches the protected mapping helpers, which is where the plot's brain is and
// which a renderer assertion could not reach anyway.
class TestGraph : public Graph {
public:
    using Graph::Graph;
    using Graph::effective_range;
    using Graph::filled_eighths;
    using Graph::visible_range;
};

void test_values_and_ring() {
    TestGraph graph;
    graph.set_values({1.0, 2.0, 3.0});
    assert(graph.get_values().size() == 3);
    assert(graph.get_values()[2] == 3.0);

    // Unbounded by default: every push is kept.
    graph.set_values({});
    graph.set_capacity(0);
    graph.push_value(1.0);
    graph.push_value(2.0);
    graph.push_value(3.0);
    graph.push_value(4.0);
    assert(graph.get_values().size() == 4);

    // A capacity turns push_value into a ring that keeps the newest samples.
    graph.set_capacity(3);
    assert(graph.get_capacity() == 3);
    graph.set_values({});
    graph.push_value(1.0);
    graph.push_value(2.0);
    graph.push_value(3.0);
    graph.push_value(4.0);
    assert(graph.get_values().size() == 3);
    assert(graph.get_values()[0] == 2.0);
    assert(graph.get_values()[1] == 3.0);
    assert(graph.get_values()[2] == 4.0);
    graph.push_value(5.0);
    assert(graph.get_values()[0] == 3.0);
    assert(graph.get_values()[2] == 5.0);

    // set_values is an explicit override: the capacity does not trim it.
    graph.set_values({1.0, 2.0, 3.0, 4.0, 5.0});
    assert(graph.get_values().size() == 5);

    // A non-positive capacity means unbounded again.
    graph.set_capacity(-1);
    graph.set_values({});
    for (int i = 0; i < 10; ++i) graph.push_value(static_cast<double>(i));
    assert(graph.get_values().size() == 10);
}

void test_visible_range() {
    TestGraph graph;
    std::vector<double> values;
    for (int i = 0; i < 10; ++i) values.push_back(static_cast<double>(i));
    graph.set_values(values);

    std::size_t begin = 0;
    std::size_t end = 0;

    // More samples than columns: the window keeps the newest (the tail).
    graph.visible_range(4, begin, end);
    assert(begin == 6 && end == 10);

    // Fewer samples than columns: the whole series, drawn right-aligned.
    graph.visible_range(20, begin, end);
    assert(begin == 0 && end == 10);

    // Degenerate width shows nothing.
    graph.visible_range(0, begin, end);
    assert(begin == 10 && end == 10);
}

void test_range_automatic_and_fixed() {
    TestGraph graph;
    double lo = 0.0;
    double hi = 0.0;

    // Auto: the span of the visible slice.
    graph.set_values({3.0, 7.0, 1.0, 5.0});
    graph.effective_range(0, 4, lo, hi);
    assert(lo == 1.0 && hi == 7.0);

    // Auto only measures the slice it is given.
    graph.effective_range(1, 3, lo, hi);
    assert(lo == 1.0 && hi == 7.0);

    // A constant series has no span; it is widened to a band around the value so
    // the line sits mid-height rather than dividing by zero.
    graph.set_values({5.0, 5.0});
    graph.effective_range(0, 2, lo, hi);
    assert(lo == 4.5 && hi == 5.5);

    // No data: a plain unit band.
    graph.set_values({});
    graph.effective_range(0, 0, lo, hi);
    assert(lo == 0.0 && hi == 1.0);

    // Fixed range wins over the data and is not widened.
    graph.set_values({3.0, 7.0});
    graph.set_range(0.0, 100.0);
    assert(graph.has_range());
    graph.effective_range(0, 2, lo, hi);
    assert(lo == 0.0 && hi == 100.0);

    // A range handed the wrong way round is normalised.
    graph.set_range(10.0, -10.0);
    graph.effective_range(0, 2, lo, hi);
    assert(lo == -10.0 && hi == 10.0);

    // A zero-width fixed range is widened like the auto case.
    graph.set_range(2.0, 2.0);
    graph.effective_range(0, 2, lo, hi);
    assert(lo == 1.5 && hi == 2.5);

    // Cleared: back to auto.
    graph.clear_range();
    assert(!graph.has_range());
    graph.set_values({3.0, 7.0});
    graph.effective_range(0, 2, lo, hi);
    assert(lo == 3.0 && hi == 7.0);
}

void test_level_math() {
    // One row is eight levels: the sparkline case.
    assert(TestGraph::filled_eighths(0.0, 0.0, 1.0, 1) == 0);
    assert(TestGraph::filled_eighths(0.5, 0.0, 1.0, 1) == 4);
    assert(TestGraph::filled_eighths(1.0, 0.0, 1.0, 1) == 8);

    // Two rows is sixteen levels; half height is exactly the top of the bottom row.
    assert(TestGraph::filled_eighths(0.5, 0.0, 1.0, 2) == 8);
    assert(TestGraph::filled_eighths(1.0, 0.0, 1.0, 2) == 16);
    assert(TestGraph::filled_eighths(0.25, 0.0, 1.0, 2) == 4);

    // Values outside the band clamp to the ends.
    assert(TestGraph::filled_eighths(-5.0, 0.0, 1.0, 3) == 0);
    assert(TestGraph::filled_eighths(5.0, 0.0, 1.0, 3) == 24);

    // A degenerate span centres the value - the same answer the widened band gives.
    assert(TestGraph::filled_eighths(9.0, 9.0, 9.0, 1) == 4);

    // A non-positive row count has nowhere to draw.
    assert(TestGraph::filled_eighths(0.5, 0.0, 1.0, 0) == 0);
}

void test_minimum_size() {
    Graph graph;
    // No width of its own; at least one row tall.
    assert((graph.get_minimum_size() == Vec2{0, 1}));

    graph.set_custom_minimum_size({6, 0});
    assert((graph.get_minimum_size() == Vec2{6, 1}));

    graph.set_custom_minimum_size({6, 4});
    assert((graph.get_minimum_size() == Vec2{6, 4}));
}

void draw_graph(Graph& graph, Node& root) {
    root.update_layout(Rect::from_size(SCREEN));
    ConsoleRenderer renderer(SCREEN, nullptr);
    graph.draw(renderer, Rect::from_size(SCREEN));
}

void test_draw_smoke() {
    auto root = std::make_unique<Node>("root");
    NodeRef<Graph> graph = root->add_child<Graph>("graph", Vec2{0, 0});

    // Empty series: still paints its rect, draws no bars.
    graph->set_size({20, 4});
    draw_graph(*graph.get(), *root);

    // A one-row sparkline over real data, then a taller filled plot.
    graph->set_values({0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0});
    graph->set_size({20, 1});
    draw_graph(*graph.get(), *root);

    // More samples than columns, so the window scrolls; auto range.
    graph->set_values({});
    for (int i = 0; i < 100; ++i) graph->push_value(static_cast<double>(i % 17));
    graph->set_size({8, 5});
    draw_graph(*graph.get(), *root);

    // Line-only over the same data.
    graph->set_fill(false);
    draw_graph(*graph.get(), *root);

    // Fixed range, a flat series (degenerate auto range) and a background colour.
    graph->set_values({42.0, 42.0, 42.0});
    graph->set_range(0.0, 100.0);
    graph->set_line_color("BRIGHT_GREEN");
    graph->set_background_color("BLACK");
    graph->set_size({10, 2});
    draw_graph(*graph.get(), *root);

    graph->clear_range();
    draw_graph(*graph.get(), *root);
}

} // namespace

int main() {
    test_values_and_ring();
    test_visible_range();
    test_range_automatic_and_fixed();
    test_level_math();
    test_minimum_size();
    test_draw_smoke();

    std::cout << "graph: all checks passed." << std::endl;
    return 0;
}
