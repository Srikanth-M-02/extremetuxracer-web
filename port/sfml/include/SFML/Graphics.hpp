/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Minimal re-implementation of the SFML 2 Graphics module API subset used
by ETR. Rendering goes through the fixed-function compatibility layer
(port/glcompat) so it shares GL state handling with the game's own
OpenGL code; images use stb_image and fonts use stb_truetype.

The semantics that ETR's layout code depends on (text bounds and
baseline placement, findCharacterPos, sprite texture rects, shape
outlines, render-texture flipping) follow SFML 2.5.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_SFML_GRAPHICS_HPP
#define ETR_SFML_GRAPHICS_HPP

#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sf {

// ---------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------
class Color {
public:
	constexpr Color() : r(0), g(0), b(0), a(255) {}
	constexpr Color(Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) : r(R), g(G), b(B), a(A) {}
	explicit Color(Uint32 color)
		: r((color & 0xff000000) >> 24), g((color & 0x00ff0000) >> 16), b((color & 0x0000ff00) >> 8), a(color & 0xff) {}
	Uint32 toInteger() const { return (Uint32(r) << 24) | (Uint32(g) << 16) | (Uint32(b) << 8) | a; }

	static const Color Black, White, Red, Green, Blue, Yellow, Magenta, Cyan, Transparent;

	Uint8 r, g, b, a;
};

inline bool operator==(const Color& l, const Color& r) { return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a; }
inline bool operator!=(const Color& l, const Color& r) { return !(l == r); }
inline Color operator+(const Color& l, const Color& r) {
	return Color(Uint8(std::min(int(l.r) + r.r, 255)), Uint8(std::min(int(l.g) + r.g, 255)),
	             Uint8(std::min(int(l.b) + r.b, 255)), Uint8(std::min(int(l.a) + r.a, 255)));
}
inline Color operator-(const Color& l, const Color& r) {
	return Color(Uint8(std::max(int(l.r) - r.r, 0)), Uint8(std::max(int(l.g) - r.g, 0)),
	             Uint8(std::max(int(l.b) - r.b, 0)), Uint8(std::max(int(l.a) - r.a, 0)));
}
inline Color operator*(const Color& l, const Color& r) {
	return Color(Uint8(int(l.r) * r.r / 255), Uint8(int(l.g) * r.g / 255), Uint8(int(l.b) * r.b / 255), Uint8(int(l.a) * r.a / 255));
}

// ---------------------------------------------------------------------
// Rect
// ---------------------------------------------------------------------
template <typename T>
class Rect {
public:
	Rect() : left(0), top(0), width(0), height(0) {}
	Rect(T l, T t, T w, T h) : left(l), top(t), width(w), height(h) {}
	Rect(const Vector2<T>& pos, const Vector2<T>& size) : left(pos.x), top(pos.y), width(size.x), height(size.y) {}
	template <typename U>
	explicit Rect(const Rect<U>& r) : left(static_cast<T>(r.left)), top(static_cast<T>(r.top)), width(static_cast<T>(r.width)), height(static_cast<T>(r.height)) {}

	bool contains(T x, T y) const {
		T minX = std::min(left, static_cast<T>(left + width)), maxX = std::max(left, static_cast<T>(left + width));
		T minY = std::min(top, static_cast<T>(top + height)), maxY = std::max(top, static_cast<T>(top + height));
		return (x >= minX) && (x < maxX) && (y >= minY) && (y < maxY);
	}
	bool contains(const Vector2<T>& p) const { return contains(p.x, p.y); }
	bool intersects(const Rect<T>& r) const {
		T l1 = std::min(left, static_cast<T>(left + width)), r1 = std::max(left, static_cast<T>(left + width));
		T t1 = std::min(top, static_cast<T>(top + height)), b1 = std::max(top, static_cast<T>(top + height));
		T l2 = std::min(r.left, static_cast<T>(r.left + r.width)), r2 = std::max(r.left, static_cast<T>(r.left + r.width));
		T t2 = std::min(r.top, static_cast<T>(r.top + r.height)), b2 = std::max(r.top, static_cast<T>(r.top + r.height));
		return std::max(l1, l2) < std::min(r1, r2) && std::max(t1, t2) < std::min(b1, b2);
	}
	Vector2<T> getPosition() const { return Vector2<T>(left, top); }
	Vector2<T> getSize() const { return Vector2<T>(width, height); }

	T left, top, width, height;
};

template <typename T> bool operator==(const Rect<T>& l, const Rect<T>& r) {
	return l.left == r.left && l.top == r.top && l.width == r.width && l.height == r.height;
}
template <typename T> bool operator!=(const Rect<T>& l, const Rect<T>& r) { return !(l == r); }

typedef Rect<int> IntRect;
typedef Rect<float> FloatRect;

// ---------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------
class Transform {
public:
	Transform();
	Transform(float a00, float a01, float a02, float a10, float a11, float a12, float a20, float a21, float a22);
	const float* getMatrix() const { return m; }
	Transform getInverse() const;
	Vector2f transformPoint(float x, float y) const;
	Vector2f transformPoint(const Vector2f& p) const { return transformPoint(p.x, p.y); }
	FloatRect transformRect(const FloatRect& rect) const;
	Transform& combine(const Transform& t);
	Transform& translate(float x, float y);
	Transform& translate(const Vector2f& o) { return translate(o.x, o.y); }
	Transform& rotate(float angle);
	Transform& rotate(float angle, float cx, float cy);
	Transform& scale(float sx, float sy);
	Transform& scale(float sx, float sy, float cx, float cy);
	Transform& scale(const Vector2f& f) { return scale(f.x, f.y); }
	static const Transform Identity;
private:
	float m[16];
};

inline Transform operator*(const Transform& l, const Transform& r) { return Transform(l).combine(r); }
inline Transform& operator*=(Transform& l, const Transform& r) { return l.combine(r); }
inline Vector2f operator*(const Transform& l, const Vector2f& r) { return l.transformPoint(r); }

class Transformable {
public:
	Transformable();
	virtual ~Transformable() {}
	void setPosition(float x, float y);
	void setPosition(const Vector2f& p) { setPosition(p.x, p.y); }
	void setRotation(float angle);
	void setScale(float fx, float fy);
	void setScale(const Vector2f& f) { setScale(f.x, f.y); }
	void setOrigin(float x, float y);
	void setOrigin(const Vector2f& o) { setOrigin(o.x, o.y); }
	const Vector2f& getPosition() const { return position; }
	float getRotation() const { return rotation; }
	const Vector2f& getScale() const { return scaleF; }
	const Vector2f& getOrigin() const { return origin; }
	void move(float ox, float oy) { setPosition(position.x + ox, position.y + oy); }
	void move(const Vector2f& o) { move(o.x, o.y); }
	void rotate(float angle) { setRotation(rotation + angle); }
	void scale(float fx, float fy) { setScale(scaleF.x * fx, scaleF.y * fy); }
	const Transform& getTransform() const;
	const Transform& getInverseTransform() const;
private:
	Vector2f origin, position;
	float rotation;
	Vector2f scaleF;
	mutable Transform transform, inverse;
	mutable bool needUpdate, inverseNeedUpdate;
};

// ---------------------------------------------------------------------
// Blending / render states
// ---------------------------------------------------------------------
struct BlendMode {
	enum Preset { Alpha = 0, Add, Multiply, None };
	BlendMode() : preset(Alpha) {}
	explicit BlendMode(Preset p) : preset(p) {}
	Preset preset;
};
inline bool operator==(const BlendMode& l, const BlendMode& r) { return l.preset == r.preset; }
inline bool operator!=(const BlendMode& l, const BlendMode& r) { return l.preset != r.preset; }
extern const BlendMode BlendAlpha;
extern const BlendMode BlendAdd;
extern const BlendMode BlendMultiply;
extern const BlendMode BlendNone;

class Texture;
class Shader;

class RenderStates {
public:
	RenderStates() : blendMode(BlendAlpha), transform(), texture(nullptr), shader(nullptr) {}
	RenderStates(const BlendMode& b) : blendMode(b), transform(), texture(nullptr), shader(nullptr) {}
	RenderStates(const Transform& t) : blendMode(BlendAlpha), transform(t), texture(nullptr), shader(nullptr) {}
	RenderStates(const Texture* tex) : blendMode(BlendAlpha), transform(), texture(tex), shader(nullptr) {}
	RenderStates(const BlendMode& b, const Transform& t, const Texture* tex, const Shader* s)
		: blendMode(b), transform(t), texture(tex), shader(s) {}
	static const RenderStates Default;
	BlendMode blendMode;
	Transform transform;
	const Texture* texture;
	const Shader* shader;
};

// ---------------------------------------------------------------------
// Vertex / primitives
// ---------------------------------------------------------------------
enum PrimitiveType { Points, Lines, LineStrip, Triangles, TriangleStrip, TriangleFan, Quads,
                     LinesStrip = LineStrip, TrianglesStrip = TriangleStrip, TrianglesFan = TriangleFan };

class Vertex {
public:
	Vertex() : position(), color(255, 255, 255), texCoords() {}
	Vertex(const Vector2f& p) : position(p), color(255, 255, 255), texCoords() {}
	Vertex(const Vector2f& p, const Color& c) : position(p), color(c), texCoords() {}
	Vertex(const Vector2f& p, const Vector2f& t) : position(p), color(255, 255, 255), texCoords(t) {}
	Vertex(const Vector2f& p, const Color& c, const Vector2f& t) : position(p), color(c), texCoords(t) {}
	Vector2f position;
	Color color;
	Vector2f texCoords;
};

class RenderTarget;

class Drawable {
public:
	virtual ~Drawable() {}
protected:
	friend class RenderTarget;
	virtual void draw(RenderTarget& target, RenderStates states) const = 0;
};

class VertexArray : public Drawable {
public:
	VertexArray() : primitive(Points) {}
	explicit VertexArray(PrimitiveType type, std::size_t count = 0) : vertices(count), primitive(type) {}
	std::size_t getVertexCount() const { return vertices.size(); }
	Vertex& operator[](std::size_t i) { return vertices[i]; }
	const Vertex& operator[](std::size_t i) const { return vertices[i]; }
	void clear() { vertices.clear(); }
	void resize(std::size_t n) { vertices.resize(n); }
	void append(const Vertex& v) { vertices.push_back(v); }
	void setPrimitiveType(PrimitiveType t) { primitive = t; }
	PrimitiveType getPrimitiveType() const { return primitive; }
	FloatRect getBounds() const;
protected:
	void draw(RenderTarget& target, RenderStates states) const override;
private:
	std::vector<Vertex> vertices;
	PrimitiveType primitive;
};

// ---------------------------------------------------------------------
// Image / Texture
// ---------------------------------------------------------------------
class Image {
public:
	void create(unsigned int width, unsigned int height, const Color& color = Color(0, 0, 0));
	void create(unsigned int width, unsigned int height, const Uint8* pixels);
	bool loadFromFile(const std::string& filename);
	bool loadFromMemory(const void* data, std::size_t size);
	bool saveToFile(const std::string& filename) const;
	Vector2u getSize() const { return size; }
	void setPixel(unsigned int x, unsigned int y, const Color& color);
	Color getPixel(unsigned int x, unsigned int y) const;
	const Uint8* getPixelsPtr() const { return pixels.empty() ? nullptr : pixels.data(); }
	void flipHorizontally();
	void flipVertically();
private:
	Vector2u size;
	std::vector<Uint8> pixels;
};

class Texture {
public:
	enum CoordinateType { Normalized, Pixels };
	Texture();
	Texture(const Texture& copy);
	~Texture();
	Texture& operator=(const Texture& right);

	bool create(unsigned int width, unsigned int height);
	bool loadFromFile(const std::string& filename, const IntRect& area = IntRect());
	bool loadFromMemory(const void* data, std::size_t size, const IntRect& area = IntRect());
	bool loadFromImage(const Image& image, const IntRect& area = IntRect());
	Vector2u getSize() const { return size; }
	Image copyToImage() const;
	void update(const Uint8* pixels);
	void update(const Uint8* pixels, unsigned int width, unsigned int height, unsigned int x, unsigned int y);
	void update(const Image& image) { update(image.getPixelsPtr(), image.getSize().x, image.getSize().y, 0, 0); }
	void update(const Image& image, unsigned int x, unsigned int y) { update(image.getPixelsPtr(), image.getSize().x, image.getSize().y, x, y); }
	void update(const Window& window);
	void setSmooth(bool smooth);
	bool isSmooth() const { return smooth; }
	void setRepeated(bool repeated);
	bool isRepeated() const { return repeated; }
	bool generateMipmap();
	unsigned int getNativeHandle() const { return id; }
	static void bind(const Texture* texture, CoordinateType coordinateType = Normalized);
	static unsigned int getMaximumSize();

	// Port extension: true for render textures (rows stored bottom-up).
	bool isFlipped() const { return flipped; }

private:
	friend class RenderTexture;
	friend class Font;
	void applyParams() const;
	void ensureId();
	Vector2u size;
	unsigned int id;
	bool smooth, repeated, flipped, mipmap;
};

// ---------------------------------------------------------------------
// Font / Glyph
// ---------------------------------------------------------------------
class Glyph {
public:
	Glyph() : advance(0) {}
	float advance;
	FloatRect bounds;
	IntRect textureRect;
};

class Font {
public:
	struct Info { std::string family; };
	Font();
	~Font();
	Font(const Font&);
	Font& operator=(const Font&);
	bool loadFromFile(const std::string& filename);
	bool loadFromMemory(const void* data, std::size_t size);
	const Info& getInfo() const;
	const Glyph& getGlyph(Uint32 codePoint, unsigned int characterSize, bool bold, float outlineThickness = 0) const;
	float getKerning(Uint32 first, Uint32 second, unsigned int characterSize) const;
	float getLineSpacing(unsigned int characterSize) const;
	float getUnderlinePosition(unsigned int characterSize) const;
	float getUnderlineThickness(unsigned int characterSize) const;
	const Texture& getTexture(unsigned int characterSize) const;
	struct Impl;
private:
	std::shared_ptr<Impl> impl;
};

// ---------------------------------------------------------------------
// Drawables
// ---------------------------------------------------------------------
class Text : public Drawable, public Transformable {
public:
	enum Style { Regular = 0, Bold = 1 << 0, Italic = 1 << 1, Underlined = 1 << 2, StrikeThrough = 1 << 3 };
	Text();
	Text(const String& string, const Font& font, unsigned int characterSize = 30);
	void setString(const String& string);
	void setFont(const Font& font);
	void setCharacterSize(unsigned int size);
	void setLineSpacing(float factor);
	void setLetterSpacing(float factor);
	void setStyle(Uint32 style);
	void setColor(const Color& color) { setFillColor(color); }
	void setFillColor(const Color& color);
	void setOutlineColor(const Color& color) { outlineColor = color; }
	void setOutlineThickness(float thickness) { outlineThickness = thickness; }
	const String& getString() const { return string; }
	const Font* getFont() const { return font; }
	unsigned int getCharacterSize() const { return characterSize; }
	Uint32 getStyle() const { return style; }
	const Color& getFillColor() const { return fillColor; }
	const Color& getColor() const { return fillColor; }
	const Color& getOutlineColor() const { return outlineColor; }
	float getOutlineThickness() const { return outlineThickness; }
	Vector2f findCharacterPos(std::size_t index) const;
	FloatRect getLocalBounds() const;
	FloatRect getGlobalBounds() const;
protected:
	void draw(RenderTarget& target, RenderStates states) const override;
private:
	void ensureGeometryUpdate() const;
	String string;
	const Font* font;
	unsigned int characterSize;
	float letterSpacingFactor, lineSpacingFactor;
	Uint32 style;
	Color fillColor, outlineColor;
	float outlineThickness;
	mutable std::vector<Vertex> vertices;
	mutable FloatRect bounds;
	mutable bool geometryNeedUpdate;
};

class Sprite : public Drawable, public Transformable {
public:
	Sprite();
	explicit Sprite(const Texture& texture);
	Sprite(const Texture& texture, const IntRect& rectangle);
	void setTexture(const Texture& texture, bool resetRect = false);
	void setTextureRect(const IntRect& rectangle);
	void setColor(const Color& color) { color_ = color; }
	const Texture* getTexture() const { return texture; }
	const IntRect& getTextureRect() const { return textureRect; }
	const Color& getColor() const { return color_; }
	FloatRect getLocalBounds() const;
	FloatRect getGlobalBounds() const;
protected:
	void draw(RenderTarget& target, RenderStates states) const override;
private:
	const Texture* texture;
	IntRect textureRect;
	Color color_;
};

class Shape : public Drawable, public Transformable {
public:
	virtual ~Shape() {}
	void setTexture(const Texture* texture, bool resetRect = false);
	void setTextureRect(const IntRect& rect) { textureRect = rect; }
	void setFillColor(const Color& color) { fillColor = color; }
	void setOutlineColor(const Color& color) { outlineColor = color; }
	void setOutlineThickness(float thickness) { outlineThickness = thickness; }
	const Texture* getTexture() const { return texture; }
	const IntRect& getTextureRect() const { return textureRect; }
	const Color& getFillColor() const { return fillColor; }
	const Color& getOutlineColor() const { return outlineColor; }
	float getOutlineThickness() const { return outlineThickness; }
	virtual std::size_t getPointCount() const = 0;
	virtual Vector2f getPoint(std::size_t index) const = 0;
	FloatRect getLocalBounds() const;
	FloatRect getGlobalBounds() const;
protected:
	Shape();
	void draw(RenderTarget& target, RenderStates states) const override;
private:
	const Texture* texture;
	IntRect textureRect;
	Color fillColor, outlineColor;
	float outlineThickness;
};

class RectangleShape : public Shape {
public:
	explicit RectangleShape(const Vector2f& size = Vector2f(0, 0)) : size_(size) {}
	void setSize(const Vector2f& size) { size_ = size; }
	const Vector2f& getSize() const { return size_; }
	std::size_t getPointCount() const override { return 4; }
	Vector2f getPoint(std::size_t index) const override;
private:
	Vector2f size_;
};

class CircleShape : public Shape {
public:
	explicit CircleShape(float radius = 0, std::size_t pointCount = 30) : radius_(radius), points_(pointCount) {}
	void setRadius(float radius) { radius_ = radius; }
	float getRadius() const { return radius_; }
	void setPointCount(std::size_t count) { points_ = count; }
	std::size_t getPointCount() const override { return points_; }
	Vector2f getPoint(std::size_t index) const override;
private:
	float radius_;
	std::size_t points_;
};

// ---------------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------------
class RenderTarget {
public:
	virtual ~RenderTarget() {}
	void clear(const Color& color = Color(0, 0, 0, 255));
	void draw(const Drawable& drawable, const RenderStates& states = RenderStates::Default);
	void draw(const Vertex* vertices, std::size_t vertexCount, PrimitiveType type,
	          const RenderStates& states = RenderStates::Default);
	virtual Vector2u getSize() const = 0;
	void pushGLStates();
	void popGLStates();
	void resetGLStates();
protected:
	RenderTarget() : statesPushed(false) {}
	virtual void bindTarget() = 0;
	virtual void unbindTarget() {}
private:
	bool statesPushed;
};

class RenderWindow : public Window, public RenderTarget {
public:
	RenderWindow() {}
	Vector2u getSize() const override { return Window::getSize(); }
	// Port extension: capture the next presented frame as a PNG download.
	void requestScreenshot(const std::string& filename) const;
protected:
	void bindTarget() override;
};

class RenderTexture : public RenderTarget {
public:
	RenderTexture();
	~RenderTexture();
	bool create(unsigned int width, unsigned int height, bool depthBuffer = false);
	bool create(unsigned int width, unsigned int height, const ContextSettings& settings) {
		return create(width, height, settings.depthBits > 0);
	}
	void setSmooth(bool smooth) { texture.setSmooth(smooth); }
	bool setActive(bool = true) { return true; }
	void display() {}
	Vector2u getSize() const override { return texture.getSize(); }
	const Texture& getTexture() const { return texture; }
protected:
	void bindTarget() override;
	void unbindTarget() override;
private:
	Texture texture;
	unsigned int fbo, depthRb;
};

class Shader {};

} // namespace sf

#endif
