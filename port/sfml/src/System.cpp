/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

sf::String, sf::Time and sf::Clock.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#include <SFML/System.hpp>
#include <emscripten.h>
#include <cstring>

namespace sf {

const Time Time::Zero;

Time seconds(float amount) { return Time(static_cast<Int64>(amount * 1000000)); }
Time milliseconds(Int32 amount) { return Time(static_cast<Int64>(amount) * 1000); }
Time microseconds(Int64 amount) { return Time(amount); }

Clock::Clock() : startMs(emscripten_get_now()) {}

Time Clock::getElapsedTime() const {
	return microseconds(static_cast<Int64>((emscripten_get_now() - startMs) * 1000.0));
}

Time Clock::restart() {
	const double now = emscripten_get_now();
	Time elapsed = microseconds(static_cast<Int64>((now - startMs) * 1000.0));
	startMs = now;
	return elapsed;
}

void sleep(Time) {
	// Blocking is not possible on the browser main thread; the game
	// loop is driven by requestAnimationFrame instead.
}

// SFML converts "ANSI" strings using the current locale. ETR's data is
// UTF-8 and is converted explicitly with fromUtf8(), so plain strings are
// treated as Latin-1 here, which matches SFML's behaviour in the "C"
// locale.
String::String(const char* ansiString) {
	if (ansiString) {
		const std::size_t len = std::strlen(ansiString);
		str.reserve(len);
		for (std::size_t i = 0; i < len; i++) str += static_cast<char32_t>(static_cast<unsigned char>(ansiString[i]));
	}
}

String::String(const std::string& ansiString) {
	str.reserve(ansiString.size());
	for (char c : ansiString) str += static_cast<char32_t>(static_cast<unsigned char>(c));
}

String::String(const wchar_t* wideString) {
	if (wideString) {
		for (; *wideString; ++wideString) str += static_cast<char32_t>(*wideString);
	}
}

String::String(const std::wstring& wideString) {
	for (wchar_t c : wideString) str += static_cast<char32_t>(c);
}

String::String(const Uint32* utf32String) {
	if (utf32String)
		for (; *utf32String; ++utf32String) str += static_cast<char32_t>(*utf32String);
}

std::string String::toAnsiString() const {
	std::string out;
	out.reserve(str.size());
	for (char32_t c : str) out += (c < 256) ? static_cast<char>(c) : '?';
	return out;
}

std::wstring String::toWideString() const {
	std::wstring out;
	out.reserve(str.size());
	for (char32_t c : str) out += static_cast<wchar_t>(c);
	return out;
}

std::string String::toUtf8() const {
	std::string out;
	out.reserve(str.size());
	for (char32_t ch : str) {
		const Uint32 c = static_cast<Uint32>(ch);
		if (c < 0x80) {
			out += static_cast<char>(c);
		} else if (c < 0x800) {
			out += static_cast<char>(0xC0 | (c >> 6));
			out += static_cast<char>(0x80 | (c & 0x3F));
		} else if (c < 0x10000) {
			out += static_cast<char>(0xE0 | (c >> 12));
			out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (c & 0x3F));
		} else {
			out += static_cast<char>(0xF0 | (c >> 18));
			out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (c & 0x3F));
		}
	}
	return out;
}

void String::replace(const String& searchFor, const String& replaceWith) {
	if (searchFor.str.empty()) return;
	std::size_t pos = 0;
	while ((pos = str.find(searchFor.str, pos)) != std::u32string::npos) {
		str.replace(pos, searchFor.str.size(), replaceWith.str);
		pos += replaceWith.str.size();
	}
}

} // namespace sf
