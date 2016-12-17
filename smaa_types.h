/**
 * Copyright (C) 2016 IRIE Shinsuke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* smaa_types.h */

#ifndef SMAA_TYPES_H
#define SMAA_TYPES_H

namespace SMAA {

/*-----------------------------------------------------------------------------*/
/* Error types */

enum ERROR_TYPE {
	ERROR_NONE = 0,
	ERROR_IMAGE_SIZE_INVALID,
	ERROR_IMAGE_MEMORY_ALLOCATION_FAILED,
	ERROR_IMAGE_BROKEN,
	ERROR_IMAGE_PUT_PIXEL_COORDS_OUT_OF_RANGE,
};

/*-----------------------------------------------------------------------------*/
/* Vector types */

class Vec2;
class Vec3;
class Vec4;
class Col4;
class Int2;

class Vec2 {
public:
	float x, y;

	Vec2() { this->x = this->y = 0.0; }
	Vec2(float x) { this->x = this->y = x; }
	Vec2(float x, float y) { this->x = x; this->y = y; }

	Vec2 yx() { return Vec2(y, x); }

	Vec2 apply(float (* func)(float)) { return Vec2(func(x), func(y)); }

	operator Int2();

	Vec2 operator + (Vec2 other) { return Vec2(x + other.x, y + other.y); }
	Vec2 operator - (Vec2 other) { return Vec2(x - other.x, y - other.y); }
	Vec2 operator * (Vec2 other) { return Vec2(x * other.x, y * other.y); }
	Vec2 operator / (Vec2 other) { return Vec2(x / other.x, y / other.y); }
	Vec2 operator += (Vec2 other) { return Vec2(x += other.x, y += other.y); }
	Vec2 operator -= (Vec2 other) { return Vec2(x -= other.x, y -= other.y); }
	Vec2 operator *= (Vec2 other) { return Vec2(x *= other.x, y *= other.y); }
	Vec2 operator /= (Vec2 other) { return Vec2(x /= other.x, y /= other.y); }
};

class Vec3 {
public:
	float x, y, z, w;

	Vec3() { this->x = this->y = this->z = 0.0; }
	Vec3(float x) { this->x = this->y = this->z = x; }
	Vec3(float x, float y, float z) { this->x = x; this->y = y; this->z = z; }
	Vec3(Vec2 a, float z) { this->x = a.x; this->y = a.y; this->z = z; }

	Vec2 xy() { return Vec2(x, y); }
};

class Vec4 {
public:
	float x, y, z, w;

	Vec4() { this->x = this->y = this->z = this->w = 0.0; }
	Vec4(float x) { this->x = this->y = this->z = this->w = x; }
	Vec4(float x, float y, float z, float w) { this->x = x; this->y = y; this->z = z; this->w = w; }
	Vec4(Vec2 a, Vec2 b) { this->x = a.x; this->y = a.y; this->z = b.x; this->w = b.y; }

	Vec2 xy() { return Vec2(x, y); }
	Vec2 zw() { return Vec2(z, w); }

	operator Col4();

	Vec4 operator + (Vec4 other) { return Vec4(x + other.x, y + other.y, z + other.z, w + other.w); }
	Vec4 operator - (Vec4 other) { return Vec4(x - other.x, y - other.y, z - other.z, w - other.w); }
	Vec4 operator * (Vec4 other) { return Vec4(x * other.x, y * other.y, z * other.z, w * other.w); }
	Vec4 operator / (Vec4 other) { return Vec4(x / other.x, y / other.y, z / other.z, w / other.w); }
	Vec4 operator += (Vec4 other) { return Vec4(x += other.x, y += other.y, z += other.z, w += other.w); }
	Vec4 operator -= (Vec4 other) { return Vec4(x -= other.x, y -= other.y, z -= other.z, w -= other.w); }
	Vec4 operator *= (Vec4 other) { return Vec4(x *= other.x, y *= other.y, z *= other.z, w *= other.w); }
	Vec4 operator /= (Vec4 other) { return Vec4(x /= other.x, y /= other.y, z /= other.z, w /= other.w); }
};

class Col4 {
public:
	float r, g, b, a;

	Col4() { this->r = this->g = this->b = this->a = 0.0; }
	Col4(float x) { this->r = this->g = this->b = this->a = x; }
	Col4(float r, float g, float b, float a) { this->r = r; this->g = g; this->b = b; this->a = a; }
	Col4(Vec2 a, Vec2 b) { this->r = a.x; this->g = a.y; this->b = b.x; this->a = b.y; }

	Vec2 rg() { return Vec2(r, g); }
	Vec2 gr() { return Vec2(g, r); }
	Vec2 ba() { return Vec2(b, a); }

	operator Vec4();

	Col4 operator + (Col4 other) { return Col4(r + other.r, g + other.g, b + other.b, a + other.a); }
	Col4 operator - (Col4 other) { return Col4(r - other.r, g - other.g, b - other.b, a - other.a); }
	Col4 operator * (Col4 other) { return Col4(r * other.r, g * other.g, b * other.b, a * other.a); }
	Col4 operator / (Col4 other) { return Col4(r / other.r, g / other.g, b / other.b, a / other.a); }
	Col4 operator += (Col4 other) { return Col4(r += other.r, g += other.g, b += other.b, a += other.a); }
	Col4 operator -= (Col4 other) { return Col4(r -= other.r, g -= other.g, b -= other.b, a -= other.a); }
	Col4 operator *= (Col4 other) { return Col4(r *= other.r, g *= other.g, b *= other.b, a *= other.a); }
	Col4 operator /= (Col4 other) { return Col4(r /= other.r, g /= other.g, b /= other.b, a /= other.a); }
};

class Int2 {
public:
	int x, y;

	Int2() { this->x = this->y = 0; }
	Int2(int x) { this->x = this->y = x; }
	Int2(int x, int y) { this->x = x; this->y = y; }

	Int2 yx() { return Int2(y, x); }

	Int2 apply(int (* func)(int)) { return Int2(func(x), func(y)); }

	operator Vec2();

	Int2 operator + (Int2 other) { return Int2(x + other.x, y + other.y); }
	Int2 operator - (Int2 other) { return Int2(x - other.x, y - other.y); }
	Int2 operator * (Int2 other) { return Int2(x * other.x, y * other.y); }
	Int2 operator / (Int2 other) { return Int2(x / other.x, y / other.y); }
	Int2 operator += (Int2 other) { return Int2(x += other.x, y += other.y); }
	Int2 operator -= (Int2 other) { return Int2(x -= other.x, y -= other.y); }
	Int2 operator *= (Int2 other) { return Int2(x *= other.x, y *= other.y); }
	Int2 operator /= (Int2 other) { return Int2(x /= other.x, y /= other.y); }
};

/*-----------------------------------------------------------------------------*/
/* Base class to define getPixel() member function as a callback */

class ImageReader {
private:
	int clamp(int x, int range) { return 0 < x ? (x < range ? x : range - 1) : 0; }
protected:
	int m_width, m_height;

	bool isOutOfRange(Int2 texcoord) { return (texcoord.x < 0 || texcoord.x >= m_width ||
						   texcoord.y < 0 || texcoord.y >= m_height); }
	int clampX(int x) { return clamp(x, m_width); }
	int clampY(int y) { return clamp(y, m_height); }
public:
	ImageReader(int width, int height) : m_width(width), m_height(height) {}

	int getWidth() { return m_width; }
	int getHeight() { return m_height; }

	virtual Col4 getPixel(Int2 texcoord) {}
};

/*-----------------------------------------------------------------------------*/
/* Simple image buffer based on ImageReader */

class Image : public ImageReader {
private:
	Col4 *m_data;

public:
	Image(int width, int height) :
		ImageReader(width, height),
		m_data(NULL)
	{
		if (m_width <= 0 || m_height <= 0)
			throw ERROR_IMAGE_SIZE_INVALID;

		m_data = (Col4 *) calloc(m_width * m_height, sizeof(Col4));

		if (!m_data)
			throw ERROR_IMAGE_MEMORY_ALLOCATION_FAILED;
	}

	~Image()
	{
		if (m_data)
			free(m_data);
	}

	void putPixel(Int2 texcoord, Col4 color)
	{
		if (!m_data)
			throw ERROR_IMAGE_BROKEN;

		if (isOutOfRange(texcoord))
			throw ERROR_IMAGE_PUT_PIXEL_COORDS_OUT_OF_RANGE;

		m_data[clampX(texcoord.x) + clampY(texcoord.y) * m_width] = color;
	}

	Col4 getPixel(Int2 texcoord)
	{
		if (!m_data)
			throw ERROR_IMAGE_BROKEN;

		return m_data[clampX(texcoord.x) + clampY(texcoord.y) * m_width];
	}
};

}
#endif /* SMAA_TYPES_H */
/* smaa_types.h ends here */
