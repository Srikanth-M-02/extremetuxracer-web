/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

SFML Graphics subset: images, textures, fonts, text, sprites, shapes and
render targets, drawn through the fixed-function compatibility layer.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#include <SFML/Graphics.hpp>
#include "../../glcompat/gl_compat.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"

#include "stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace sf {

// =====================================================================
// Constants
// =====================================================================
const Color Color::Black(0, 0, 0);
const Color Color::White(255, 255, 255);
const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::Yellow(255, 255, 0);
const Color Color::Magenta(255, 0, 255);
const Color Color::Cyan(0, 255, 255);
const Color Color::Transparent(0, 0, 0, 0);

const BlendMode BlendAlpha(BlendMode::Alpha);
const BlendMode BlendAdd(BlendMode::Add);
const BlendMode BlendMultiply(BlendMode::Multiply);
const BlendMode BlendNone(BlendMode::None);

const Transform Transform::Identity;
const RenderStates RenderStates::Default;

namespace {

bool ReadFile(const std::string& filename, std::vector<unsigned char>& out) {
	std::ifstream in(filename, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

GLenum ToGLPrimitive(PrimitiveType t) {
	switch (t) {
		case Points: return GL_POINTS;
		case Lines: return GL_LINES;
		case LineStrip: return GL_LINE_STRIP;
		case Triangles: return GL_TRIANGLES;
		case TriangleStrip: return GL_TRIANGLE_STRIP;
		case TriangleFan: return GL_TRIANGLE_FAN;
		case Quads: return GL_QUADS;
		default: return GL_TRIANGLES;
	}
}

GLint CurrentTextureBinding() {
	GLint id = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
	return id;
}

} // namespace

// =====================================================================
// Transform / Transformable (same maths as SFML)
// =====================================================================
Transform::Transform() {
	std::memset(m, 0, sizeof(m));
	m[0] = m[5] = m[10] = m[15] = 1.f;
}

Transform::Transform(float a00, float a01, float a02, float a10, float a11, float a12, float a20, float a21, float a22) {
	m[0] = a00; m[4] = a01; m[8] = 0.f; m[12] = a02;
	m[1] = a10; m[5] = a11; m[9] = 0.f; m[13] = a12;
	m[2] = 0.f; m[6] = 0.f; m[10] = 1.f; m[14] = 0.f;
	m[3] = a20; m[7] = a21; m[11] = 0.f; m[15] = a22;
}

Transform Transform::getInverse() const {
	float det = m[0] * (m[15] * m[5] - m[7] * m[13]) -
	            m[1] * (m[15] * m[4] - m[7] * m[12]) +
	            m[3] * (m[13] * m[4] - m[5] * m[12]);
	if (det == 0.f) return Identity;
	return Transform((m[15] * m[5] - m[7] * m[13]) / det,
	                 -(m[15] * m[4] - m[7] * m[12]) / det,
	                 (m[13] * m[4] - m[5] * m[12]) / det,
	                 -(m[15] * m[1] - m[3] * m[13]) / det,
	                 (m[15] * m[0] - m[3] * m[12]) / det,
	                 -(m[13] * m[0] - m[1] * m[12]) / det,
	                 (m[7] * m[1] - m[3] * m[5]) / det,
	                 -(m[7] * m[0] - m[3] * m[4]) / det,
	                 (m[5] * m[0] - m[1] * m[4]) / det);
}

Vector2f Transform::transformPoint(float x, float y) const {
	return Vector2f(m[0] * x + m[4] * y + m[12], m[1] * x + m[5] * y + m[13]);
}

FloatRect Transform::transformRect(const FloatRect& r) const {
	const Vector2f pts[4] = {
		transformPoint(r.left, r.top), transformPoint(r.left, r.top + r.height),
		transformPoint(r.left + r.width, r.top), transformPoint(r.left + r.width, r.top + r.height)
	};
	float left = pts[0].x, top = pts[0].y, right = pts[0].x, bottom = pts[0].y;
	for (int i = 1; i < 4; i++) {
		left = std::min(left, pts[i].x); right = std::max(right, pts[i].x);
		top = std::min(top, pts[i].y); bottom = std::max(bottom, pts[i].y);
	}
	return FloatRect(left, top, right - left, bottom - top);
}

Transform& Transform::combine(const Transform& t) {
	const float* a = m;
	const float* b = t.m;
	*this = Transform(a[0] * b[0] + a[4] * b[1] + a[12] * b[3],
	                  a[0] * b[4] + a[4] * b[5] + a[12] * b[7],
	                  a[0] * b[12] + a[4] * b[13] + a[12] * b[15],
	                  a[1] * b[0] + a[5] * b[1] + a[13] * b[3],
	                  a[1] * b[4] + a[5] * b[5] + a[13] * b[7],
	                  a[1] * b[12] + a[5] * b[13] + a[13] * b[15],
	                  a[3] * b[0] + a[7] * b[1] + a[15] * b[3],
	                  a[3] * b[4] + a[7] * b[5] + a[15] * b[7],
	                  a[3] * b[12] + a[7] * b[13] + a[15] * b[15]);
	return *this;
}

Transform& Transform::translate(float x, float y) {
	return combine(Transform(1, 0, x, 0, 1, y, 0, 0, 1));
}

Transform& Transform::rotate(float angle) {
	const float rad = angle * 3.141592654f / 180.f;
	const float c = std::cos(rad), s = std::sin(rad);
	return combine(Transform(c, -s, 0, s, c, 0, 0, 0, 1));
}

Transform& Transform::rotate(float angle, float cx, float cy) {
	const float rad = angle * 3.141592654f / 180.f;
	const float c = std::cos(rad), s = std::sin(rad);
	return combine(Transform(c, -s, cx * (1 - c) + cy * s, s, c, cy * (1 - c) - cx * s, 0, 0, 1));
}

Transform& Transform::scale(float sx, float sy) {
	return combine(Transform(sx, 0, 0, 0, sy, 0, 0, 0, 1));
}

Transform& Transform::scale(float sx, float sy, float cx, float cy) {
	return combine(Transform(sx, 0, cx * (1 - sx), 0, sy, cy * (1 - sy), 0, 0, 1));
}

Transformable::Transformable()
	: origin(0, 0), position(0, 0), rotation(0), scaleF(1, 1), needUpdate(true), inverseNeedUpdate(true) {}

void Transformable::setPosition(float x, float y) {
	position = Vector2f(x, y);
	needUpdate = inverseNeedUpdate = true;
}

void Transformable::setRotation(float angle) {
	rotation = std::fmod(angle, 360.f);
	if (rotation < 0) rotation += 360.f;
	needUpdate = inverseNeedUpdate = true;
}

void Transformable::setScale(float fx, float fy) {
	scaleF = Vector2f(fx, fy);
	needUpdate = inverseNeedUpdate = true;
}

void Transformable::setOrigin(float x, float y) {
	origin = Vector2f(x, y);
	needUpdate = inverseNeedUpdate = true;
}

const Transform& Transformable::getTransform() const {
	if (needUpdate) {
		const float angle = -rotation * 3.141592654f / 180.f;
		const float cosine = std::cos(angle), sine = std::sin(angle);
		const float sxc = scaleF.x * cosine, syc = scaleF.y * cosine;
		const float sxs = scaleF.x * sine, sys = scaleF.y * sine;
		const float tx = -origin.x * sxc - origin.y * sys + position.x;
		const float ty = origin.x * sxs - origin.y * syc + position.y;
		transform = Transform(sxc, sys, tx, -sxs, syc, ty, 0.f, 0.f, 1.f);
		needUpdate = false;
	}
	return transform;
}

const Transform& Transformable::getInverseTransform() const {
	if (inverseNeedUpdate) {
		inverse = getTransform().getInverse();
		inverseNeedUpdate = false;
	}
	return inverse;
}

// =====================================================================
// Image
// =====================================================================
void Image::create(unsigned int width, unsigned int height, const Color& color) {
	size = Vector2u(width, height);
	pixels.resize((std::size_t)width * height * 4);
	for (std::size_t i = 0; i < pixels.size(); i += 4) {
		pixels[i] = color.r; pixels[i + 1] = color.g; pixels[i + 2] = color.b; pixels[i + 3] = color.a;
	}
}

void Image::create(unsigned int width, unsigned int height, const Uint8* data) {
	size = Vector2u(width, height);
	pixels.assign(data, data + (std::size_t)width * height * 4);
}

bool Image::loadFromMemory(const void* data, std::size_t len) {
	int w = 0, h = 0, n = 0;
	unsigned char* p = stbi_load_from_memory(static_cast<const stbi_uc*>(data), (int)len, &w, &h, &n, 4);
	if (!p) return false;
	size = Vector2u(w, h);
	pixels.assign(p, p + (std::size_t)w * h * 4);
	stbi_image_free(p);
	return true;
}

bool Image::loadFromFile(const std::string& filename) {
	std::vector<unsigned char> bytes;
	if (!ReadFile(filename, bytes) || !loadFromMemory(bytes.data(), bytes.size())) {
		std::fprintf(stderr, "Failed to load image \"%s\"\n", filename.c_str());
		return false;
	}
	return true;
}

bool Image::saveToFile(const std::string& filename) const {
	if (pixels.empty()) return false;
	return stbi_write_png(filename.c_str(), size.x, size.y, 4, pixels.data(), size.x * 4) != 0;
}

void Image::setPixel(unsigned int x, unsigned int y, const Color& c) {
	Uint8* p = &pixels[((std::size_t)y * size.x + x) * 4];
	p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

Color Image::getPixel(unsigned int x, unsigned int y) const {
	const Uint8* p = &pixels[((std::size_t)y * size.x + x) * 4];
	return Color(p[0], p[1], p[2], p[3]);
}

void Image::flipHorizontally() {
	for (unsigned int y = 0; y < size.y; y++) {
		for (unsigned int x = 0; x < size.x / 2; x++) {
			Uint8* a = &pixels[((std::size_t)y * size.x + x) * 4];
			Uint8* b = &pixels[((std::size_t)y * size.x + (size.x - 1 - x)) * 4];
			for (int c = 0; c < 4; c++) std::swap(a[c], b[c]);
		}
	}
}

void Image::flipVertically() {
	const std::size_t row = (std::size_t)size.x * 4;
	std::vector<Uint8> tmp(row);
	for (unsigned int y = 0; y < size.y / 2; y++) {
		Uint8* a = &pixels[y * row];
		Uint8* b = &pixels[(size.y - 1 - y) * row];
		std::memcpy(tmp.data(), a, row);
		std::memcpy(a, b, row);
		std::memcpy(b, tmp.data(), row);
	}
}

// =====================================================================
// Texture
// =====================================================================
Texture::Texture() : size(0, 0), id(0), smooth(false), repeated(false), flipped(false), mipmap(false) {}

Texture::Texture(const Texture& copy) : size(0, 0), id(0), smooth(copy.smooth), repeated(copy.repeated), flipped(false), mipmap(false) {
	if (copy.id) {
		Image img = copy.copyToImage();
		loadFromImage(img);
	}
}

Texture::~Texture() {
	if (id) {
		GLuint t = id;
		glDeleteTextures(1, &t);
	}
}

Texture& Texture::operator=(const Texture& right) {
	if (this == &right) return *this;
	Texture tmp(right);
	std::swap(size, tmp.size);
	std::swap(id, tmp.id);
	std::swap(smooth, tmp.smooth);
	std::swap(repeated, tmp.repeated);
	std::swap(flipped, tmp.flipped);
	std::swap(mipmap, tmp.mipmap);
	return *this;
}

void Texture::ensureId() {
	if (!id) {
		GLuint t = 0;
		glGenTextures(1, &t);
		id = t;
	}
}

void Texture::applyParams() const {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
	GLint minFilter = smooth ? GL_LINEAR : GL_NEAREST;
	if (mipmap) minFilter = smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

bool Texture::create(unsigned int width, unsigned int height) {
	if (width == 0 || height == 0) return false;
	ensureId();
	size = Vector2u(width, height);
	flipped = false;
	mipmap = false;
	const GLint prev = CurrentTextureBinding();
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	applyParams();
	glBindTexture(GL_TEXTURE_2D, prev);
	return true;
}

bool Texture::loadFromFile(const std::string& filename, const IntRect& area) {
	Image img;
	return img.loadFromFile(filename) && loadFromImage(img, area);
}

bool Texture::loadFromMemory(const void* data, std::size_t len, const IntRect& area) {
	Image img;
	return img.loadFromMemory(data, len) && loadFromImage(img, area);
}

bool Texture::loadFromImage(const Image& image, const IntRect& area) {
	const int w = (int)image.getSize().x, h = (int)image.getSize().y;
	if (area.width == 0 || area.height == 0 || (area.left <= 0 && area.top <= 0 && area.width >= w && area.height >= h)) {
		if (!create(w, h)) return false;
		update(image);
		return true;
	}
	IntRect r = area;
	if (r.left < 0) r.left = 0;
	if (r.top < 0) r.top = 0;
	if (r.left + r.width > w) r.width = w - r.left;
	if (r.top + r.height > h) r.height = h - r.top;
	if (!create(r.width, r.height)) return false;
	std::vector<Uint8> sub((std::size_t)r.width * r.height * 4);
	for (int y = 0; y < r.height; y++)
		std::memcpy(&sub[(std::size_t)y * r.width * 4], image.getPixelsPtr() + ((std::size_t)(r.top + y) * w + r.left) * 4, (std::size_t)r.width * 4);
	update(sub.data());
	return true;
}

void Texture::update(const Uint8* pixels) {
	update(pixels, size.x, size.y, 0, 0);
}

void Texture::update(const Uint8* pixels, unsigned int width, unsigned int height, unsigned int x, unsigned int y) {
	if (!id || !pixels) return;
	const GLint prev = CurrentTextureBinding();
	glBindTexture(GL_TEXTURE_2D, id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
	flipped = false;
	glBindTexture(GL_TEXTURE_2D, prev);
}

void Texture::update(const Window& window) {
	if (!id) return;
	const GLint prev = CurrentTextureBinding();
	glBindTexture(GL_TEXTURE_2D, id);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, std::min(size.x, window.getSize().x), std::min(size.y, window.getSize().y));
	flipped = true;
	glBindTexture(GL_TEXTURE_2D, prev);
}

Image Texture::copyToImage() const {
	Image img;
	if (!id) return img;
	std::vector<Uint8> pixels((std::size_t)size.x * size.y * 4);
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	img.create(size.x, size.y, pixels.data());
	if (flipped) img.flipVertically();
	return img;
}

void Texture::setSmooth(bool s) {
	smooth = s;
	if (id) {
		const GLint prev = CurrentTextureBinding();
		glBindTexture(GL_TEXTURE_2D, id);
		applyParams();
		glBindTexture(GL_TEXTURE_2D, prev);
	}
}

void Texture::setRepeated(bool r) {
	repeated = r;
	if (id) {
		const GLint prev = CurrentTextureBinding();
		glBindTexture(GL_TEXTURE_2D, id);
		applyParams();
		glBindTexture(GL_TEXTURE_2D, prev);
	}
}

bool Texture::generateMipmap() {
	if (!id) return false;
	const GLint prev = CurrentTextureBinding();
	glBindTexture(GL_TEXTURE_2D, id);
	glGenerateMipmap(GL_TEXTURE_2D);
	mipmap = true;
	applyParams();
	glBindTexture(GL_TEXTURE_2D, prev);
	return true;
}

void Texture::bind(const Texture* texture, CoordinateType) {
	glBindTexture(GL_TEXTURE_2D, texture ? texture->id : 0);
}

unsigned int Texture::getMaximumSize() {
	GLint s = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s);
	return (unsigned int)s;
}

// =====================================================================
// Font (stb_truetype, one glyph atlas per character size)
// =====================================================================
struct Font::Impl {
	struct Page {
		std::map<Uint32, Glyph> glyphs;
		std::vector<Uint8> alpha;
		unsigned int width = 512, height = 128;
		unsigned int penX = 0, penY = 0, rowHeight = 0;
		bool dirty = true;
		Texture texture;
	};
	std::vector<unsigned char> data;
	stbtt_fontinfo info;
	Font::Info fontInfo;
	int ascent = 0, descent = 0, lineGap = 0;
	std::map<unsigned int, Page> pages;

	float Scale(unsigned int size) const { return stbtt_ScaleForMappingEmToPixels(&info, (float)size); }

	Page& GetPage(unsigned int size) {
		Page& p = pages[size];
		if (p.alpha.empty()) p.alpha.assign((std::size_t)p.width * p.height, 0);
		return p;
	}

	const Glyph& GetGlyph(Uint32 cp, unsigned int size) {
		Page& page = GetPage(size);
		std::map<Uint32, Glyph>::iterator it = page.glyphs.find(cp);
		if (it != page.glyphs.end()) return it->second;

		Glyph g;
		const int gi = stbtt_FindGlyphIndex(&info, (int)cp);
		const float scale = Scale(size);
		int adv = 0, lsb = 0;
		stbtt_GetGlyphHMetrics(&info, gi, &adv, &lsb);
		g.advance = std::round(adv * scale);

		int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		stbtt_GetGlyphBitmapBox(&info, gi, scale, scale, &x0, &y0, &x1, &y1);
		const int w = x1 - x0, h = y1 - y0;
		if (w > 0 && h > 0) {
			const unsigned int padding = 2;
			if (page.penX + w + padding > page.width) {
				page.penX = 0;
				page.penY += page.rowHeight;
				page.rowHeight = 0;
			}
			while (page.penY + h + padding > page.height && page.height < 4096) {
				page.height *= 2;
				page.alpha.resize((std::size_t)page.width * page.height, 0);
			}
			const unsigned int px = page.penX + 1, py = page.penY + 1;
			if (py + h <= page.height) {
				stbtt_MakeGlyphBitmap(&info, &page.alpha[(std::size_t)py * page.width + px], w, h, page.width, scale, scale, gi);
				g.textureRect = IntRect(px, py, w, h);
				g.bounds = FloatRect((float)x0, (float)y0, (float)w, (float)h);
				page.penX += w + padding;
				page.rowHeight = std::max(page.rowHeight, (unsigned int)h + padding);
				page.dirty = true;
			}
		}
		return page.glyphs.emplace(cp, g).first->second;
	}

	const Texture& GetTexture(unsigned int size) {
		Page& page = GetPage(size);
		if (page.dirty) {
			if (page.texture.getSize().x != page.width || page.texture.getSize().y != page.height) {
				page.texture.setSmooth(true);
				page.texture.create(page.width, page.height);
			}
			std::vector<Uint8> rgba((std::size_t)page.width * page.height * 4);
			for (std::size_t i = 0; i < page.alpha.size(); i++) {
				rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
				rgba[i * 4 + 3] = page.alpha[i];
			}
			page.texture.update(rgba.data());
			page.dirty = false;
		}
		return page.texture;
	}
};

Font::Font() {}
Font::~Font() {}
Font::Font(const Font& other) : impl(other.impl) {}
Font& Font::operator=(const Font& other) { impl = other.impl; return *this; }

bool Font::loadFromMemory(const void* data, std::size_t len) {
	std::shared_ptr<Impl> p = std::make_shared<Impl>();
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	p->data.assign(bytes, bytes + len);
	if (!stbtt_InitFont(&p->info, p->data.data(), stbtt_GetFontOffsetForIndex(p->data.data(), 0)))
		return false;
	stbtt_GetFontVMetrics(&p->info, &p->ascent, &p->descent, &p->lineGap);
	impl = p;
	return true;
}

bool Font::loadFromFile(const std::string& filename) {
	std::vector<unsigned char> bytes;
	if (!ReadFile(filename, bytes) || !loadFromMemory(bytes.data(), bytes.size())) {
		std::fprintf(stderr, "Failed to load font \"%s\"\n", filename.c_str());
		return false;
	}
	impl->fontInfo.family = filename;
	return true;
}

const Font::Info& Font::getInfo() const {
	static Info empty;
	return impl ? impl->fontInfo : empty;
}

const Glyph& Font::getGlyph(Uint32 codePoint, unsigned int characterSize, bool, float) const {
	static Glyph empty;
	if (!impl) return empty;
	return impl->GetGlyph(codePoint, characterSize);
}

float Font::getKerning(Uint32 first, Uint32 second, unsigned int characterSize) const {
	if (!impl || first == 0 || second == 0) return 0.f;
	return stbtt_GetCodepointKernAdvance(&impl->info, (int)first, (int)second) * impl->Scale(characterSize);
}

float Font::getLineSpacing(unsigned int characterSize) const {
	if (!impl) return 0.f;
	return std::round((impl->ascent - impl->descent + impl->lineGap) * impl->Scale(characterSize));
}

float Font::getUnderlinePosition(unsigned int characterSize) const {
	return impl ? characterSize * 0.1f : 0.f;
}

float Font::getUnderlineThickness(unsigned int characterSize) const {
	return impl ? std::max(1.f, characterSize / 14.f) : 0.f;
}

const Texture& Font::getTexture(unsigned int characterSize) const {
	static Texture empty;
	if (!impl) return empty;
	return impl->GetTexture(characterSize);
}

// =====================================================================
// Text
// =====================================================================
Text::Text()
	: font(nullptr), characterSize(30), letterSpacingFactor(1.f), lineSpacingFactor(1.f), style(Regular),
	  fillColor(255, 255, 255), outlineColor(0, 0, 0), outlineThickness(0), geometryNeedUpdate(false) {}

Text::Text(const String& s, const Font& f, unsigned int size)
	: string(s), font(&f), characterSize(size), letterSpacingFactor(1.f), lineSpacingFactor(1.f), style(Regular),
	  fillColor(255, 255, 255), outlineColor(0, 0, 0), outlineThickness(0), geometryNeedUpdate(true) {}

void Text::setString(const String& s) {
	if (string != s) {
		string = s;
		geometryNeedUpdate = true;
	}
}

void Text::setFont(const Font& f) {
	if (font != &f) {
		font = &f;
		geometryNeedUpdate = true;
	}
}

void Text::setCharacterSize(unsigned int size) {
	if (characterSize != size) {
		characterSize = size;
		geometryNeedUpdate = true;
	}
}

void Text::setLineSpacing(float factor) { lineSpacingFactor = factor; geometryNeedUpdate = true; }
void Text::setLetterSpacing(float factor) { letterSpacingFactor = factor; geometryNeedUpdate = true; }
void Text::setStyle(Uint32 s) { style = s; geometryNeedUpdate = true; }

void Text::setFillColor(const Color& color) {
	if (color != fillColor) {
		fillColor = color;
		for (Vertex& v : vertices) v.color = color;
	}
}

Vector2f Text::findCharacterPos(std::size_t index) const {
	if (!font) return Vector2f();
	if (index > string.getSize()) index = string.getSize();
	const float whitespace = font->getGlyph(L' ', characterSize, false).advance;
	const float letterSpacing = (whitespace / 3.f) * (letterSpacingFactor - 1.f);
	const float hspace = whitespace + letterSpacing;
	const float vspace = font->getLineSpacing(characterSize) * lineSpacingFactor;
	Vector2f pos;
	Uint32 prev = 0;
	for (std::size_t i = 0; i < index; i++) {
		const Uint32 cur = string[i];
		pos.x += font->getKerning(prev, cur, characterSize);
		prev = cur;
		switch (cur) {
			case ' ': pos.x += hspace; continue;
			case '\t': pos.x += hspace * 4; continue;
			case '\n': pos.y += vspace; pos.x = 0; continue;
			default: break;
		}
		pos.x += font->getGlyph(cur, characterSize, false).advance + letterSpacing;
	}
	return getTransform().transformPoint(pos);
}

FloatRect Text::getLocalBounds() const {
	ensureGeometryUpdate();
	return bounds;
}

FloatRect Text::getGlobalBounds() const {
	return getTransform().transformRect(getLocalBounds());
}

void Text::ensureGeometryUpdate() const {
	if (!font || !geometryNeedUpdate) return;
	geometryNeedUpdate = false;
	vertices.clear();
	bounds = FloatRect();
	if (string.isEmpty()) return;

	const float whitespace = font->getGlyph(L' ', characterSize, false).advance;
	const float letterSpacing = (whitespace / 3.f) * (letterSpacingFactor - 1.f);
	const float whitespaceWidth = whitespace + letterSpacing;
	const float lineSpacing = font->getLineSpacing(characterSize) * lineSpacingFactor;
	float x = 0.f;
	float y = (float)characterSize;
	float minX = (float)characterSize, minY = (float)characterSize, maxX = 0.f, maxY = 0.f;
	Uint32 prev = 0;
	for (std::size_t i = 0; i < string.getSize(); i++) {
		const Uint32 cur = string[i];
		if (cur == '\r') continue;
		x += font->getKerning(prev, cur, characterSize);
		prev = cur;
		if (cur == ' ' || cur == '\n' || cur == '\t') {
			minX = std::min(minX, x);
			minY = std::min(minY, y);
			switch (cur) {
				case ' ': x += whitespaceWidth; break;
				case '\t': x += whitespaceWidth * 4; break;
				case '\n': y += lineSpacing; x = 0; break;
			}
			maxX = std::max(maxX, x);
			maxY = std::max(maxY, y);
			continue;
		}
		const Glyph& g = font->getGlyph(cur, characterSize, false);
		if (g.textureRect.width > 0) {
			const float padding = 1.f;
			const float left = g.bounds.left - padding, top = g.bounds.top - padding;
			const float right = g.bounds.left + g.bounds.width + padding;
			const float bottom = g.bounds.top + g.bounds.height + padding;
			const float u1 = g.textureRect.left - padding, v1 = g.textureRect.top - padding;
			const float u2 = g.textureRect.left + g.textureRect.width + padding;
			const float v2 = g.textureRect.top + g.textureRect.height + padding;
			vertices.push_back(Vertex(Vector2f(x + left, y + top), fillColor, Vector2f(u1, v1)));
			vertices.push_back(Vertex(Vector2f(x + right, y + top), fillColor, Vector2f(u2, v1)));
			vertices.push_back(Vertex(Vector2f(x + left, y + bottom), fillColor, Vector2f(u1, v2)));
			vertices.push_back(Vertex(Vector2f(x + left, y + bottom), fillColor, Vector2f(u1, v2)));
			vertices.push_back(Vertex(Vector2f(x + right, y + top), fillColor, Vector2f(u2, v1)));
			vertices.push_back(Vertex(Vector2f(x + right, y + bottom), fillColor, Vector2f(u2, v2)));
		}
		minX = std::min(minX, x + g.bounds.left);
		maxX = std::max(maxX, x + g.bounds.left + g.bounds.width);
		minY = std::min(minY, y + g.bounds.top);
		maxY = std::max(maxY, y + g.bounds.top + g.bounds.height);
		x += g.advance + letterSpacing;
	}
	bounds = FloatRect(minX, minY, maxX - minX, maxY - minY);
}

void Text::draw(RenderTarget& target, RenderStates states) const {
	if (!font) return;
	ensureGeometryUpdate();
	if (vertices.empty()) return;
	states.transform *= getTransform();
	states.texture = &font->getTexture(characterSize);
	target.draw(vertices.data(), vertices.size(), Triangles, states);
}

// =====================================================================
// Sprite
// =====================================================================
Sprite::Sprite() : texture(nullptr), textureRect(), color_(255, 255, 255) {}

Sprite::Sprite(const Texture& tex) : texture(nullptr), textureRect(), color_(255, 255, 255) {
	setTexture(tex, true);
}

Sprite::Sprite(const Texture& tex, const IntRect& rect) : texture(nullptr), textureRect(), color_(255, 255, 255) {
	setTexture(tex);
	setTextureRect(rect);
}

void Sprite::setTexture(const Texture& tex, bool resetRect) {
	if (resetRect || (!texture && textureRect == IntRect()))
		setTextureRect(IntRect(0, 0, tex.getSize().x, tex.getSize().y));
	texture = &tex;
}

void Sprite::setTextureRect(const IntRect& rect) { textureRect = rect; }

FloatRect Sprite::getLocalBounds() const {
	return FloatRect(0.f, 0.f, (float)std::abs(textureRect.width), (float)std::abs(textureRect.height));
}

FloatRect Sprite::getGlobalBounds() const {
	return getTransform().transformRect(getLocalBounds());
}

void Sprite::draw(RenderTarget& target, RenderStates states) const {
	if (!texture) return;
	const FloatRect b = getLocalBounds();
	const float left = (float)textureRect.left;
	const float right = left + textureRect.width;
	const float top = (float)textureRect.top;
	const float bottom = top + textureRect.height;
	const Vertex v[4] = {
		Vertex(Vector2f(0, 0), color_, Vector2f(left, top)),
		Vertex(Vector2f(0, b.height), color_, Vector2f(left, bottom)),
		Vertex(Vector2f(b.width, 0), color_, Vector2f(right, top)),
		Vertex(Vector2f(b.width, b.height), color_, Vector2f(right, bottom)),
	};
	states.transform *= getTransform();
	states.texture = texture;
	target.draw(v, 4, TriangleStrip, states);
}

// =====================================================================
// Shapes
// =====================================================================
Shape::Shape() : texture(nullptr), textureRect(), fillColor(255, 255, 255), outlineColor(255, 255, 255), outlineThickness(0) {}

void Shape::setTexture(const Texture* tex, bool resetRect) {
	if (tex && (resetRect || (!texture && textureRect == IntRect())))
		textureRect = IntRect(0, 0, tex->getSize().x, tex->getSize().y);
	texture = tex;
}

namespace {

FloatRect PointBounds(const std::vector<Vector2f>& pts) {
	if (pts.empty()) return FloatRect();
	float left = pts[0].x, top = pts[0].y, right = pts[0].x, bottom = pts[0].y;
	for (const Vector2f& p : pts) {
		left = std::min(left, p.x); right = std::max(right, p.x);
		top = std::min(top, p.y); bottom = std::max(bottom, p.y);
	}
	return FloatRect(left, top, right - left, bottom - top);
}

Vector2f ComputeNormal(const Vector2f& p1, const Vector2f& p2) {
	Vector2f n(p1.y - p2.y, p2.x - p1.x);
	const float len = std::sqrt(n.x * n.x + n.y * n.y);
	if (len != 0.f) n /= len;
	return n;
}

void ShapeGeometry(const Shape& s, std::vector<Vector2f>& pts, std::vector<Vector2f>& outline) {
	const std::size_t count = s.getPointCount();
	pts.clear();
	outline.clear();
	for (std::size_t i = 0; i < count; i++) pts.push_back(s.getPoint(i));
	if (count < 3 || s.getOutlineThickness() == 0.f) return;
	const FloatRect inside = PointBounds(pts);
	const Vector2f center(inside.left + inside.width / 2, inside.top + inside.height / 2);
	for (std::size_t i = 0; i < count; i++) {
		const Vector2f& p0 = (i == 0) ? pts[count - 1] : pts[i - 1];
		const Vector2f& p1 = pts[i];
		const Vector2f& p2 = (i == count - 1) ? pts[0] : pts[i + 1];
		Vector2f n1 = ComputeNormal(p0, p1);
		Vector2f n2 = ComputeNormal(p1, p2);
		const Vector2f toCenter = center - p1;
		if (n1.x * toCenter.x + n1.y * toCenter.y > 0) n1 = -n1;
		if (n2.x * toCenter.x + n2.y * toCenter.y > 0) n2 = -n2;
		const float factor = 1.f + (n1.x * n2.x + n1.y * n2.y);
		const Vector2f normal = (n1 + n2) / factor;
		outline.push_back(p1);
		outline.push_back(p1 + normal * s.getOutlineThickness());
	}
	outline.push_back(outline[0]);
	outline.push_back(outline[1]);
}

} // namespace

FloatRect Shape::getLocalBounds() const {
	std::vector<Vector2f> pts, outline;
	ShapeGeometry(*this, pts, outline);
	return outline.empty() ? PointBounds(pts) : PointBounds(outline);
}

FloatRect Shape::getGlobalBounds() const {
	return getTransform().transformRect(getLocalBounds());
}

void Shape::draw(RenderTarget& target, RenderStates states) const {
	std::vector<Vector2f> pts, outline;
	ShapeGeometry(*this, pts, outline);
	if (pts.size() < 3) return;
	states.transform *= getTransform();

	const FloatRect inside = PointBounds(pts);
	std::vector<Vertex> fill;
	fill.reserve(pts.size() + 2);
	auto texCoord = [&](const Vector2f& p) {
		const float xr = inside.width > 0 ? (p.x - inside.left) / inside.width : 0.f;
		const float yr = inside.height > 0 ? (p.y - inside.top) / inside.height : 0.f;
		return Vector2f(textureRect.left + textureRect.width * xr, textureRect.top + textureRect.height * yr);
	};
	const Vector2f center(inside.left + inside.width / 2, inside.top + inside.height / 2);
	fill.push_back(Vertex(center, fillColor, texCoord(center)));
	for (const Vector2f& p : pts) fill.push_back(Vertex(p, fillColor, texCoord(p)));
	fill.push_back(fill[1]);

	RenderStates fillStates = states;
	fillStates.texture = texture;
	target.draw(fill.data(), fill.size(), TriangleFan, fillStates);

	if (!outline.empty()) {
		std::vector<Vertex> ov;
		ov.reserve(outline.size());
		for (const Vector2f& p : outline) ov.push_back(Vertex(p, outlineColor));
		RenderStates outlineStates = states;
		outlineStates.texture = nullptr;
		target.draw(ov.data(), ov.size(), TriangleStrip, outlineStates);
	}
}

Vector2f RectangleShape::getPoint(std::size_t index) const {
	switch (index) {
		default:
		case 0: return Vector2f(0, 0);
		case 1: return Vector2f(size_.x, 0);
		case 2: return Vector2f(size_.x, size_.y);
		case 3: return Vector2f(0, size_.y);
	}
}

Vector2f CircleShape::getPoint(std::size_t index) const {
	const float angle = index * 2.f * 3.141592654f / points_ - 3.141592654f / 2.f;
	return Vector2f(radius_ + std::cos(angle) * radius_, radius_ + std::sin(angle) * radius_);
}

// =====================================================================
// VertexArray
// =====================================================================
FloatRect VertexArray::getBounds() const {
	if (vertices.empty()) return FloatRect();
	float left = vertices[0].position.x, top = vertices[0].position.y;
	float right = left, bottom = top;
	for (const Vertex& v : vertices) {
		left = std::min(left, v.position.x); right = std::max(right, v.position.x);
		top = std::min(top, v.position.y); bottom = std::max(bottom, v.position.y);
	}
	return FloatRect(left, top, right - left, bottom - top);
}

void VertexArray::draw(RenderTarget& target, RenderStates states) const {
	if (!vertices.empty()) target.draw(vertices.data(), vertices.size(), primitive, states);
}

// =====================================================================
// RenderTarget
// =====================================================================
void RenderTarget::clear(const Color& color) {
	const bool selfContained = !statesPushed;
	if (selfContained) glc::PushAllState();
	bindTarget();
	glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
	glClear(GL_COLOR_BUFFER_BIT);
	unbindTarget();
	if (selfContained) glc::PopAllState();
}

void RenderTarget::draw(const Drawable& drawable, const RenderStates& states) {
	drawable.draw(*this, states);
}

void RenderTarget::draw(const Vertex* vertices, std::size_t vertexCount, PrimitiveType type, const RenderStates& states) {
	if (!vertices || vertexCount == 0) return;
	const bool selfContained = !statesPushed;
	if (selfContained) glc::PushAllState();
	bindTarget();
	glc::ResetForSFML();

	static std::vector<glc::Vertex2D> verts;
	verts.resize(vertexCount);
	const Texture* tex = states.texture;
	const bool hasTex = tex && tex->getNativeHandle() && tex->getSize().x && tex->getSize().y;
	const float invW = hasTex ? 1.f / tex->getSize().x : 0.f;
	const float invH = hasTex ? 1.f / tex->getSize().y : 0.f;
	const bool flip = hasTex && tex->isFlipped();
	for (std::size_t i = 0; i < vertexCount; i++) {
		const Vertex& s = vertices[i];
		glc::Vertex2D& d = verts[i];
		d.x = s.position.x;
		d.y = s.position.y;
		d.r = s.color.r; d.g = s.color.g; d.b = s.color.b; d.a = s.color.a;
		d.u = s.texCoords.x * invW;
		d.v = flip ? 1.f - s.texCoords.y * invH : s.texCoords.y * invH;
	}

	const Vector2u size = getSize();
	float proj[16] = {0};
	proj[0] = 2.f / size.x;
	proj[5] = -2.f / size.y;
	proj[10] = -1.f;
	proj[12] = -1.f;
	proj[13] = 1.f;
	proj[15] = 1.f;

	glc::BlendPreset blend = glc::BlendAlpha;
	switch (states.blendMode.preset) {
		case BlendMode::Add: blend = glc::BlendAdd; break;
		case BlendMode::Multiply: blend = glc::BlendMultiply; break;
		case BlendMode::None: blend = glc::BlendNone; break;
		default: break;
	}
	glc::Draw2D(verts.data(), (int)vertexCount, ToGLPrimitive(type), proj, states.transform.getMatrix(),
	            hasTex ? tex->getNativeHandle() : 0, blend);

	unbindTarget();
	if (selfContained) glc::PopAllState();
}

void RenderTarget::pushGLStates() {
	glc::PushAllState();
	glc::ResetForSFML();
	statesPushed = true;
}

void RenderTarget::popGLStates() {
	glc::PopAllState();
	statesPushed = false;
}

void RenderTarget::resetGLStates() { glc::ResetForSFML(); }

void RenderWindow::bindTarget() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	const Vector2u s = getSize();
	glViewport(0, 0, s.x, s.y);
}

RenderTexture::RenderTexture() : fbo(0), depthRb(0) {}

RenderTexture::~RenderTexture() {
	if (depthRb) { GLuint r = depthRb; glDeleteRenderbuffers(1, &r); }
	if (fbo) { GLuint f = fbo; glDeleteFramebuffers(1, &f); }
}

bool RenderTexture::create(unsigned int width, unsigned int height, bool depthBuffer) {
	if (!texture.create(width, height)) return false;
	texture.setSmooth(false);
	texture.flipped = true;
	if (!fbo) {
		GLuint f = 0;
		glGenFramebuffers(1, &f);
		fbo = f;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getNativeHandle(), 0);
	if (depthBuffer) {
		if (!depthRb) {
			GLuint r = 0;
			glGenRenderbuffers(1, &r);
			depthRb = r;
		}
		glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRb);
	}
	const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void RenderTexture::bindTarget() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	const Vector2u s = getSize();
	glViewport(0, 0, s.x, s.y);
}

void RenderTexture::unbindTarget() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace sf
