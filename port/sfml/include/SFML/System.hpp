/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Minimal re-implementation of the SFML 2 System module API subset used
by ETR (sf::String, sf::Vector2/3, sf::Time, sf::Clock).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_SFML_SYSTEM_HPP
#define ETR_SFML_SYSTEM_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <iterator>

namespace sf {

typedef std::int8_t Int8;
typedef std::uint8_t Uint8;
typedef std::int16_t Int16;
typedef std::uint16_t Uint16;
typedef std::int32_t Int32;
typedef std::uint32_t Uint32;
typedef std::int64_t Int64;
typedef std::uint64_t Uint64;

// ---------------------------------------------------------------------
// Vectors
// ---------------------------------------------------------------------
template <typename T>
class Vector2 {
public:
	T x, y;
	Vector2() : x(0), y(0) {}
	Vector2(T X, T Y) : x(X), y(Y) {}
	template <typename U>
	explicit Vector2(const Vector2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}
};

template <typename T> Vector2<T> operator-(const Vector2<T>& r) { return Vector2<T>(-r.x, -r.y); }
template <typename T> Vector2<T>& operator+=(Vector2<T>& l, const Vector2<T>& r) { l.x += r.x; l.y += r.y; return l; }
template <typename T> Vector2<T>& operator-=(Vector2<T>& l, const Vector2<T>& r) { l.x -= r.x; l.y -= r.y; return l; }
template <typename T> Vector2<T> operator+(const Vector2<T>& l, const Vector2<T>& r) { return Vector2<T>(l.x + r.x, l.y + r.y); }
template <typename T> Vector2<T> operator-(const Vector2<T>& l, const Vector2<T>& r) { return Vector2<T>(l.x - r.x, l.y - r.y); }
template <typename T> Vector2<T> operator*(const Vector2<T>& l, T r) { return Vector2<T>(l.x * r, l.y * r); }
template <typename T> Vector2<T> operator*(T l, const Vector2<T>& r) { return Vector2<T>(r.x * l, r.y * l); }
template <typename T> Vector2<T>& operator*=(Vector2<T>& l, T r) { l.x *= r; l.y *= r; return l; }
template <typename T> Vector2<T> operator/(const Vector2<T>& l, T r) { return Vector2<T>(l.x / r, l.y / r); }
template <typename T> Vector2<T>& operator/=(Vector2<T>& l, T r) { l.x /= r; l.y /= r; return l; }
template <typename T> bool operator==(const Vector2<T>& l, const Vector2<T>& r) { return l.x == r.x && l.y == r.y; }
template <typename T> bool operator!=(const Vector2<T>& l, const Vector2<T>& r) { return !(l == r); }

typedef Vector2<int> Vector2i;
typedef Vector2<unsigned int> Vector2u;
typedef Vector2<float> Vector2f;

template <typename T>
class Vector3 {
public:
	T x, y, z;
	Vector3() : x(0), y(0), z(0) {}
	Vector3(T X, T Y, T Z) : x(X), y(Y), z(Z) {}
	template <typename U>
	explicit Vector3(const Vector3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}
};

template <typename T> Vector3<T> operator+(const Vector3<T>& l, const Vector3<T>& r) { return Vector3<T>(l.x + r.x, l.y + r.y, l.z + r.z); }
template <typename T> Vector3<T> operator-(const Vector3<T>& l, const Vector3<T>& r) { return Vector3<T>(l.x - r.x, l.y - r.y, l.z - r.z); }
template <typename T> Vector3<T> operator*(const Vector3<T>& l, T r) { return Vector3<T>(l.x * r, l.y * r, l.z * r); }
template <typename T> bool operator==(const Vector3<T>& l, const Vector3<T>& r) { return l.x == r.x && l.y == r.y && l.z == r.z; }
template <typename T> bool operator!=(const Vector3<T>& l, const Vector3<T>& r) { return !(l == r); }

typedef Vector3<int> Vector3i;
typedef Vector3<float> Vector3f;

// ---------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------
class Time {
public:
	Time() : us(0) {}
	float asSeconds() const { return us / 1000000.f; }
	Int32 asMilliseconds() const { return static_cast<Int32>(us / 1000); }
	Int64 asMicroseconds() const { return us; }
	static const Time Zero;
private:
	friend Time seconds(float);
	friend Time milliseconds(Int32);
	friend Time microseconds(Int64);
	explicit Time(Int64 micro) : us(micro) {}
	Int64 us;
};

Time seconds(float amount);
Time milliseconds(Int32 amount);
Time microseconds(Int64 amount);
inline bool operator<(Time l, Time r) { return l.asMicroseconds() < r.asMicroseconds(); }
inline bool operator>(Time l, Time r) { return l.asMicroseconds() > r.asMicroseconds(); }
inline Time operator-(Time l, Time r) { return microseconds(l.asMicroseconds() - r.asMicroseconds()); }
inline Time operator+(Time l, Time r) { return microseconds(l.asMicroseconds() + r.asMicroseconds()); }

class Clock {
public:
	Clock();
	Time getElapsedTime() const;
	Time restart();
private:
	double startMs;
};

void sleep(Time duration);

// ---------------------------------------------------------------------
// String (UTF-32 storage, like SFML)
// ---------------------------------------------------------------------
class String {
public:
	typedef std::u32string::iterator Iterator;
	typedef std::u32string::const_iterator ConstIterator;
	static const std::size_t InvalidPos = std::u32string::npos;

	String() {}
	String(char ansiChar) : str(1, static_cast<char32_t>(static_cast<unsigned char>(ansiChar))) {}
	String(wchar_t wideChar) : str(1, static_cast<char32_t>(wideChar)) {}
	String(Uint32 utf32Char) : str(1, static_cast<char32_t>(utf32Char)) {}
	String(const char* ansiString);
	String(const std::string& ansiString);
	String(const wchar_t* wideString);
	String(const std::wstring& wideString);
	String(const Uint32* utf32String);
	String(const std::u32string& utf32String) : str(utf32String) {}

	template <typename T>
	static String fromUtf8(T begin, T end);
	template <typename T>
	static String fromUtf16(T begin, T end) {
		String s;
		for (; begin != end; ++begin) s.str += static_cast<char32_t>(static_cast<Uint16>(*begin));
		return s;
	}
	template <typename T>
	static String fromUtf32(T begin, T end) {
		String s;
		s.str.assign(begin, end);
		return s;
	}

	operator std::string() const { return toAnsiString(); }
	operator std::wstring() const { return toWideString(); }

	std::string toAnsiString() const;
	std::wstring toWideString() const;
	std::string toUtf8() const;
	std::basic_string<char32_t> toUtf32() const { return str; }

	String& operator=(const String& right) = default;
	String& operator+=(const String& right) { str += right.str; return *this; }
	Uint32 operator[](std::size_t index) const { return str[index]; }
	char32_t& operator[](std::size_t index) { return str[index]; }

	void clear() { str.clear(); }
	std::size_t getSize() const { return str.size(); }
	bool isEmpty() const { return str.empty(); }
	void erase(std::size_t position, std::size_t count = 1) { str.erase(position, count); }
	void insert(std::size_t position, const String& s) { str.insert(position, s.str); }
	std::size_t find(const String& s, std::size_t start = 0) const { return str.find(s.str, start); }
	void replace(std::size_t position, std::size_t length, const String& replaceWith) { str.replace(position, length, replaceWith.str); }
	void replace(const String& searchFor, const String& replaceWith);
	String substring(std::size_t position, std::size_t length = InvalidPos) const { return String(str.substr(position, length)); }
	const char32_t* getData() const { return str.c_str(); }

	Iterator begin() { return str.begin(); }
	ConstIterator begin() const { return str.begin(); }
	Iterator end() { return str.end(); }
	ConstIterator end() const { return str.end(); }

	friend bool operator==(const String& l, const String& r) { return l.str == r.str; }
	friend bool operator!=(const String& l, const String& r) { return l.str != r.str; }
	friend bool operator<(const String& l, const String& r) { return l.str < r.str; }
	friend bool operator>(const String& l, const String& r) { return l.str > r.str; }
	friend String operator+(const String& l, const String& r) { String s(l); s += r; return s; }

private:
	std::u32string str;
};

template <typename T>
String String::fromUtf8(T begin, T end) {
	String s;
	while (begin != end) {
		Uint32 c = static_cast<unsigned char>(*begin++);
		int extra = 0;
		if (c >= 0xF0) { c &= 0x07; extra = 3; }
		else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
		else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
		else if (c >= 0x80) { c = 0xFFFD; }
		for (int i = 0; i < extra; i++) {
			if (begin == end) { c = 0xFFFD; break; }
			c = (c << 6) | (static_cast<unsigned char>(*begin++) & 0x3F);
		}
		s.str += static_cast<char32_t>(c);
	}
	return s;
}

} // namespace sf

#endif
