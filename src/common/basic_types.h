#ifndef INTEGRAL_TYPES_H
#define INTEGRAL_TYPES_H

#include <stdint.h>
#include <chrono>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t ;
using u64 = uint64_t ;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t ;

using f32 = float;

// Define macros for binary arithmetic operators definitions, to reduce code duplication.
#define VEC2D_ARITH_MACRO(op) \
	constexpr vec2d<T>& operator op##= (const vec2d<T>& rhs) {\
		x op##= rhs.x;\
		y op##= rhs.y;\
		return *this;\
	}\
	constexpr friend vec2d<T> operator op (vec2d<T> lhs, const vec2d<T>& rhs) {\
		lhs op##= rhs;\
		return lhs;\
	}

#define VEC2D_SCALAR_ARITH_MACRO(op) \
	constexpr vec2d<T>& operator op##= (const int rhs) {\
		x op##= rhs;\
		y op##= rhs;\
		return *this;\
	}\
	constexpr friend vec2d<T> operator op (vec2d<T> lhs, const int rhs) {\
		lhs op##= rhs;\
		return lhs;\
	}

#define VEC2D_COMP_MACRO(op) \
    constexpr bool operator op (const vec2d<T>& rhs) const { return x op rhs.x && y op rhs.y; }

template <typename T>
struct vec2d {
	T x = 0, y = 0;
	constexpr vec2d  (const T x_in = 0, const T y_in = 0) noexcept : x(x_in), y(y_in) {};

	template <typename U>
	constexpr vec2d<U> to() const { return vec2d<U>(static_cast<U>(x), static_cast<U>(y)); }

	// Expand operator definitions for the given types
    VEC2D_COMP_MACRO(!=)
    VEC2D_COMP_MACRO(==)
    VEC2D_COMP_MACRO(<)
    VEC2D_COMP_MACRO(>)

	VEC2D_ARITH_MACRO(+)
    VEC2D_ARITH_MACRO(-)
    VEC2D_ARITH_MACRO(*)
    VEC2D_ARITH_MACRO(/)

    VEC2D_SCALAR_ARITH_MACRO(+)
	VEC2D_SCALAR_ARITH_MACRO(-)
	VEC2D_SCALAR_ARITH_MACRO(*)
	VEC2D_SCALAR_ARITH_MACRO(/)
};

using screen_coord_t = u16;
using screen_coords = vec2d<screen_coord_t>;
using world_coord_t = f32;
using world_coords = vec2d<world_coord_t>;
using sprite_coord_t = f32;
using sprite_coords = vec2d<sprite_coord_t>;

template <typename T>
using size = vec2d<T>;

template <typename T>
using point = vec2d<T>;

template <typename T>
struct rect {
	constexpr rect() noexcept = default;
	constexpr rect(point<T> p, ::size<T> s) noexcept : size(s), origin(p)  {};

	constexpr vec2d<T> top_right() { return vec2d<T>(origin.x + size.x, origin.y); }
	constexpr vec2d<T> bottom_right() { return vec2d<T>(origin.x + size.x, origin.y + size.y); }
	constexpr vec2d<T> bottom_left() { return vec2d<T>(origin.x, origin.y + size.y); }
	constexpr vec2d<T> center() { return vec2d<T>(origin.x + size.x / 2, origin.y + size.y / 2);};

	::size<T> size {};
	point<T> origin {};
};

template <typename T>
bool test_range(T a, T b, T c) {
	return ((b > a) && (b < c));
}

template <typename T>
bool test_collision(rect<T> r, vec2d<T> p) {
	bool x = test_range(r.origin.x, p.x, r.bottom_right().x);
	bool y = test_range(r.origin.y, p.y, r.bottom_right().y);

	return (x && y);
}

template <typename T>
bool AABB_collision(rect<T> a, rect<T> b) {
	return (a.origin.x <= b.top_right().x && a.top_right().x >= b.origin.x) &&
		   (a.origin.y <= b.bottom_left().y && a.bottom_left().y >= b.origin.y);
}

struct no_copy {
	no_copy() = default;
	no_copy (no_copy&) = delete;
	no_copy& operator = (no_copy&) = delete;
};

struct no_move {
	no_move() = default;
	no_move (no_move&&) = delete;
	no_move& operator = (no_move&&) = delete;
};

using entity = u32;
constexpr static entity null_entity = 65535;

class timer {
	std::chrono::steady_clock::time_point _start;
public:
	using ms = std::chrono::milliseconds;
	using seconds = std::chrono::seconds;
	using microseconds = std::chrono::microseconds;

	template <typename T>
	T elapsed() { return std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - _start); }
	void start() { _start = std::chrono::steady_clock::now(); }
	timer() { start(); }
};

#endif //INTEGRAL_TYPES_H