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


template <typename T>
struct point {
	T x = 0, y = 0;
	constexpr point  (const T x_in = 0, const T y_in = 0) noexcept : x(x_in), y(y_in) {};

	template <typename U>
	constexpr point<U> to() { return point<U>(static_cast<U>(x), static_cast<U>(y)); }
	constexpr bool operator == (const point<T>& rhs) const {
		return x == rhs.x && y == rhs.y;
	}
	constexpr point<T>& operator += (const point<T>& rhs) {
		x = x + rhs.x;
		y = y + rhs.y;
		return *this;
	}
	constexpr friend point<T> operator + (point<T> lhs, const point<T>& rhs) {
		lhs += rhs;
		return lhs;
	}
	constexpr point<T>& operator -= (const point<T>& rhs) {
		x = x - rhs.x;
		y = y - rhs.y;
		return *this;
	}
	constexpr friend point<T> operator - (point<T> lhs, const point<T>& rhs) {
		lhs -= rhs;
		return lhs;
	}
};

template <typename T>
struct size {
	T w = 0, h = 0;
	constexpr size(const T w_in = 0, const T h_in = 0) noexcept : w(w_in), h(h_in) {};

	template <typename U>
	constexpr size<U> to() { return size<U>(static_cast<U>(w), static_cast<U>(h)); }
};


template <typename T>
struct rect {
private:
	// Alias, in order to use "size" as member name
	using size_type = ::size<T>;
public:
	constexpr rect() noexcept = default;
	constexpr rect(point<T> p, size_type s) noexcept : size(s), origin(p)  {};
	constexpr rect(point<T> tl, point<T> br) noexcept {
		origin = tl;
		origin = point<T>(br.x - tl.x, br.y - tl.y);
	};

	constexpr point<T> top_right() { return point<T>(origin.x + size.w, origin.y); }
	//constexpr point<T> bottom_left() { return origin; }
	constexpr point<T> bottom_right() { return point<T>(origin.x + size.w, origin.y + size.h); }
	constexpr point<T> bottom_left() { return point<T>(origin.x, origin.y + size.h); }

	constexpr point<T> center() { return point<T>(origin.x + size.w / 2, origin.y + size.h / 2);};
	size_type size {};
	point<T> origin {};
};





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

class timer {
    std::chrono::steady_clock::time_point _start;
public:
    using ms = std::chrono::milliseconds;
    using seconds = std::chrono::seconds;
    using microseconds = std::chrono::microseconds;

    template <typename T>
    T elapsed() {
        return  std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - _start);
    }
    void start() {
        _start = std::chrono::steady_clock::now();
    }
    timer() { start(); }
};

#endif //INTEGRAL_TYPES_H
