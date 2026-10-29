#ifndef GUAYABA_NODES_NODETEXTMATRIX_HPP
#define GUAYABA_NODES_NODETEXTMATRIX_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <guayaba/Core/Vec2.hpp>
#include <guayaba/Nodes/Node2D.hpp>
#include <guayaba/Renderer/ConsoleRenderer.hpp>

namespace guayaba {

class NodeTextMatrix : public Node2D{
protected:
    Vec2 size;
    std::vector<std::string> text;
    uint16_t text_color = 7;       // White, the TermChar default
    uint16_t background_color = 0; // Black, the TermChar default

    void recalculate_size();

public:

    NodeTextMatrix(const std::string& node_name);

    void set_text(std::vector<std::string> new_text);

    std::vector<std::string> get_text();

    bool is_inside(const Vec2 point);

    void on_draw(ConsoleRenderer& renderer, const Rect& clip) override;

    void change_text_color(const std::string& text_color_name);

    void change_background_color(const std::string& background_color_name);

    Vec2 get_size() const;
};

} // namespace guayaba

#endif // GUAYABA_NODES_NODETEXTMATRIX_HPP