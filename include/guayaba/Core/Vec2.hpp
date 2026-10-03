#ifndef GUAYABA_CORE_VEC2_HPP
#define GUAYABA_CORE_VEC2_HPP

namespace guayaba {

// Replaces Windows COORD
struct Vec2 {
    int x = 0;
    int y = 0;
};

inline Vec2 const operator+(Vec2 a, Vec2 b){ return {a.x + b.x, a.y + b.y}; }
inline Vec2 const operator-(Vec2 a, Vec2 b){ return {a.x - b.x, a.y - b.y}; }
inline bool operator==(Vec2 a, Vec2 b){ return (a.x == b.x) && (a.y == b.y); }
inline bool operator!=(Vec2 a, Vec2 b){ return !(a == b); }

} // namespace guayaba

#endif // GUAYABA_CORE_VEC2_HPP