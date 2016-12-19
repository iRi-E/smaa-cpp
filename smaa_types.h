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
/* Base class to define getPixel() member function as a callback */

class ImageReader {

protected:
	int m_width, m_height;

	bool isOutOfRange(int x, int y) { return (x < 0 || x >= m_width || y < 0 || y >= m_height); }
	int clampX(int x) { return clamp(x, m_width); }
	int clampY(int y) { return clamp(y, m_height); }

public:
	ImageReader(int width, int height) : m_width(width), m_height(height) {}

	int getWidth() { return m_width; }
	int getHeight() { return m_height; }

	virtual void getPixel(int x, int y, float color[4]) {}

private:
	int clamp(int x, int range) { return 0 < x ? (x < range ? x : range - 1) : 0; }
};

/*-----------------------------------------------------------------------------*/
/* Simple image buffer based on ImageReader */

class Image : public ImageReader {

private:
	float *m_data;

public:
	Image(int width, int height) :
		ImageReader(width, height),
		m_data(NULL)
	{
		if (m_width <= 0 || m_height <= 0)
			throw ERROR_IMAGE_SIZE_INVALID;

		m_data = (float *) calloc(m_width * m_height * 4, sizeof(float));

		if (!m_data)
			throw ERROR_IMAGE_MEMORY_ALLOCATION_FAILED;
	}

	~Image()
	{
		if (m_data)
			free(m_data);
	}

	void putPixel(int x, int y, float color[4])
	{
		if (!m_data)
			throw ERROR_IMAGE_BROKEN;

		if (isOutOfRange(x, y))
			throw ERROR_IMAGE_PUT_PIXEL_COORDS_OUT_OF_RANGE;

		float *ptr = &m_data[(x + y * m_width) * 4];
		*ptr++ = *color++;
		*ptr++ = *color++;
		*ptr++ = *color++;
		*ptr   = *color;
	}

	void getPixel(int x, int y, float color[4])
	{
		if (!m_data)
			throw ERROR_IMAGE_BROKEN;

		float *ptr = &m_data[(clampX(x) + clampY(y) * m_width) * 4];
		*color++ = *ptr++;
		*color++ = *ptr++;
		*color++ = *ptr++;
		*color   = *ptr;
	}
};

}
#endif /* SMAA_TYPES_H */
/* smaa_types.h ends here */
