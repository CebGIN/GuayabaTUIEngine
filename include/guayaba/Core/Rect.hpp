#ifndef GUAYABA_CORE_RECT_HPP
#define GUAYABA_CORE_RECT_HPP

#include <algorithm>

#include <guayaba/Core/Vec2.hpp>

namespace guayaba {

struct Rect {
    Vec2 pos;
    Vec2 size;

    Rect() = default;

    Rect(Vec2 pos_, Vec2 size_)
        : pos(pos_), size{std::max(0, size_.x), std::max(0, size_.y)} {}

    // The rect covering (0,0) to (size), i.e. at the origin.
    static Rect from_size(Vec2 size_) { return Rect{Vec2{0, 0}, size_}; }

    // The rect a node occupies, given its global position and its size.
    static Rect from_origin(Vec2 origin, Vec2 extent) {
        return Rect{origin, extent};
    }

    int left() const { return pos.x; }
    int top() const { return pos.y; }
    int right() const { return pos.x + size.x; }
    int bottom() const { return pos.y + size.y; }

    bool is_empty() const { return size.x <= 0 || size.y <= 0; }

    Vec2 center() const { return {pos.x + size.x / 2, pos.y + size.y / 2}; }

    // Half-open, so neighbouring rects never both claim a shared edge.
    bool contains(Vec2 point) const {
        return point.x >= pos.x && point.x < pos.x + size.x &&
               point.y >= pos.y && point.y < pos.y + size.y;
    }

    bool contains(const Rect& other) const {
        return other.pos.x >= pos.x && other.pos.y >= pos.y &&
               other.pos.x + other.size.x <= pos.x + size.x &&
               other.pos.y + other.size.y <= pos.y + size.y;
    }

    bool intersects(const Rect& other) const {
        if (is_empty() || other.is_empty()) return false;
        return pos.x < other.pos.x + other.size.x &&
               other.pos.x < pos.x + size.x &&
               pos.y < other.pos.y + other.size.y &&
               other.pos.y < pos.y + size.y;
    }


    Rect intersection(const Rect& other) const {
        if (!intersects(other)) return Rect{pos, Vec2{0, 0}};
        const int x0 = std::max(pos.x, other.pos.x);
        const int y0 = std::max(pos.y, other.pos.y);
        const int x1 = std::min(pos.x + size.x, other.pos.x + other.size.x);
        const int y1 = std::min(pos.y + size.y, other.pos.y + other.size.y);
        return Rect{Vec2{x0, y0}, Vec2{x1 - x0, y1 - y0}};
    }

    Rect grown(int dx, int dy) const {
        return Rect{Vec2{pos.x - dx, pos.y - dy},
                    Vec2{size.x + dx * 2, size.y + dy * 2}};
    }

    Rect translated(Vec2 delta) const {
        return Rect{pos + delta, size};
    }

    bool operator==(const Rect& other) const {
        return pos == other.pos && size == other.size;
    }
    bool operator!=(const Rect& other) const { return !(*this == other); }
};

} // namespace guayaba

#endif // GUAYABA_CORE_RECT_HPP