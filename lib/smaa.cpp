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

/* smaa.cpp */

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "smaa.h"
#include "smaa_areatex.h"

namespace SMAA {

/*-----------------------------------------------------------------------------*/
/* Non-Configurable Defines */

static const int AREATEX_SIZE = 80; /* 20 * 4 = 80 */
static const int AREATEX_MAX_DISTANCE = 20;
static const int AREATEX_MAX_DISTANCE_DIAG = 20;
static const float RGB_WEIGHTS[3] = {0.2126f, 0.7152f, 0.0722f};

/*-----------------------------------------------------------------------------*/
/* Misc functions */

static inline float step(float edge, float x)
{
	return x < edge ? 0.0f : 1.0f;
}

static inline float saturate(float x)
{
	return 0.0f < x ? (x < 1.0f ? x : 1.0f) : 0.0f;
}

static inline float lerp(float a, float b, float p)
{
	return a + (b - a) * p;
}

static inline float bilinear(float c00, float c10, float c01, float c11, float x, float y)
{
	return (c00 * (1.0f - x) + c10 * x) * (1.0f - y) + (c01 * (1.0f - x) + c11 * x) * y;
}

static inline float rgb2bw(const float color[4])
{
	return RGB_WEIGHTS[0] * color[0] + RGB_WEIGHTS[1] * color[1] + RGB_WEIGHTS[2] * color[2];
}

static inline float color_delta(const float color1[4], const float color2[4])
{
	return fmaxf(fmaxf(fabsf(color1[0] - color2[0]), fabsf(color1[1] - color2[1])), fabsf(color1[2] - color2[2]));
}

/*-----------------------------------------------------------------------------*/
/* Internal Functions to Sample Pixel Color from Image with Bilinear Filtering */

static void sample_bilinear(ImageReader *image, float x, float y, float output[4])
{
	float ix = floorf(x), iy = floorf(y);
	float fx = x - ix, fy = y - iy;
	int X = (int)ix, Y = (int)iy;

	float color00[4], color10[4], color01[4], color11[4];

	image->getPixel(X + 0, Y + 0, color00);
	image->getPixel(X + 1, Y + 0, color10);
	image->getPixel(X + 0, Y + 1, color01);
	image->getPixel(X + 1, Y + 1, color11);

	output[0] = bilinear(color00[0], color10[0], color01[0], color11[0], fx, fy);
	output[1] = bilinear(color00[1], color10[1], color01[1], color11[1], fx, fy);
	output[2] = bilinear(color00[2], color10[2], color01[2], color11[2], fx, fy);
	output[3] = bilinear(color00[3], color10[3], color01[3], color11[3], fx, fy);
}

static void sample_bilinear_vertical(ImageReader *image, int x, int y, float yoffset, float output[4])
{
	float iy = floorf(yoffset);
	float fy = yoffset - iy;
	y += (int)iy;

	float color00[4], color01[4];

	image->getPixel(x + 0, y + 0, color00);
	image->getPixel(x + 0, y + 1, color01);

	output[0] = lerp(color00[0], color01[0], fy);
	output[1] = lerp(color00[1], color01[1], fy);
	output[2] = lerp(color00[2], color01[2], fy);
	output[3] = lerp(color00[3], color01[3], fy);
}

static void sample_bilinear_horizontal(ImageReader *image, int x, int y, float xoffset, float output[4])
{
	float ix = floorf(xoffset);
	float fx = xoffset - ix;
	x += (int)ix;

	float color00[4], color10[4];

	image->getPixel(x + 0, y + 0, color00);
	image->getPixel(x + 1, y + 0, color10);

	output[0] = lerp(color00[0], color10[0], fx);
	output[1] = lerp(color00[1], color10[1], fx);
	output[2] = lerp(color00[2], color10[2], fx);
	output[3] = lerp(color00[3], color10[3], fx);
}

/*-----------------------------------------------------------------------------*/
/* Internal Functions to Sample Blending Weights from AreaTex */

static inline int clamp_areatex_coord(int x)
{
	return 0 < x ? (x < AREATEX_SIZE ? x : AREATEX_SIZE - 1) : 0;
}

static inline const float* areatex_sample_internal(const float *areatex, int x, int y)
{
	return &areatex[(clamp_areatex_coord(x) + clamp_areatex_coord(y) * AREATEX_SIZE) * 2];
}

/**
 * We have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
static void area(int d1, int d2, int e1, int e2, int offset,
		 /* out */ float weights[2])
{
	/* The areas texture is compressed quadratically: */
	float x = (float)(AREATEX_MAX_DISTANCE * e1) + sqrtf((float)d1);
	float y = (float)(AREATEX_MAX_DISTANCE * e2) + sqrtf((float)d2);

#ifdef WITH_SUBPIXEL_RENDERING
	/* Move to proper place, according to the subpixel offset: */
	y += (float)(AREATEX_SIZE * offset);
#endif

	/* Do it! */
	float ix = floorf(x), iy = floorf(y);
	float fx = x - ix, fy = y - iy;
	int X = (int)ix, Y = (int)iy;

	const float *weights00 = areatex_sample_internal(areatex, X + 0, Y + 0);
	const float *weights10 = areatex_sample_internal(areatex, X + 1, Y + 0);
	const float *weights01 = areatex_sample_internal(areatex, X + 0, Y + 1);
	const float *weights11 = areatex_sample_internal(areatex, X + 1, Y + 1);

	weights[0] = bilinear(weights00[0], weights10[0], weights01[0], weights11[0], fx, fy);
	weights[1] = bilinear(weights00[1], weights10[1], weights01[1], weights11[1], fx, fy);
}

/**
 * Similar to area(), this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
static void area_diag(int d1, int d2, int e1, int e2, int offset,
		      /* out */ float weights[2])
{
	int x = AREATEX_MAX_DISTANCE_DIAG * e1 + d1;
	int y = AREATEX_MAX_DISTANCE_DIAG * e2 + d2;

#ifdef WITH_SUBPIXEL_RENDERING
	/* Move to proper place, according to the subpixel offset: */
	y += AREATEX_SIZE * offset;
#endif

	/* Do it! */
	const float *w = areatex_sample_internal(areatex_diag, x, y);
	weights[0] = w[0];
	weights[1] = w[1];
}

/*-----------------------------------------------------------------------------*/
/* Predicated Thresholding Used for Edge Detection */

/**
 * Adjusts the threshold by means of predication.
 */
void PixelShader::calculatePredicatedThreshold(int x, int y, ImageReader *predicationImage, float threshold[2])
{
	float here[4], left[4], top[4];

	predicationImage->getPixel(x, y, here);
	predicationImage->getPixel(x - 1, y, left);
	predicationImage->getPixel(x, y - 1, top);

	float edges[2] = {step(m_predication_threshold, fabsf(here[0] - left[0])),
			  step(m_predication_threshold, fabsf(here[0] - top[0]))};

	float scaled = m_predication_scale * m_threshold;

	threshold[0] = scaled * (1.0f - m_predication_strength * edges[0]);
	threshold[1] = scaled * (1.0f - m_predication_strength * edges[1]);
}

/*-----------------------------------------------------------------------------*/
/* Edge Detection Pixel Shaders (First Pass) */

/**
 * Luma Edge Detection
 *
 * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and
 * thus 'colorImage' should be a non-sRGB image.
 */
void PixelShader::lumaEdgeDetection(int x, int y,
				    ImageReader *colorImage,
				    ImageReader *predicationImage,
				    /* out */ float edges[4])
{
	float threshold[2];
	float color[4];

	/* Calculate the threshold: */
	if (m_enable_predication && predicationImage)
		calculatePredicatedThreshold(x, y, predicationImage, threshold);
	else
		threshold[0] = threshold[1] = m_threshold;

	/* Calculate lumas and deltas: */
	colorImage->getPixel(x, y, color);
	float L = rgb2bw(color);
	colorImage->getPixel(x - 1, y, color);
	float Lleft = rgb2bw(color);
	colorImage->getPixel(x, y - 1, color);
	float Ltop  = rgb2bw(color);
	float Dleft = fabsf(L - Lleft);
	float Dtop  = fabsf(L - Ltop);

	/* We do the usual threshold: */
	edges[0] = step(threshold[0], Dleft);
	edges[1] = step(threshold[1], Dtop);
	edges[2] = 0.0f;
	edges[3] = 1.0f;

	/* Then discard if there is no edge: */
	if (edges[0] == 0.0f && edges[1] == 0.0f)
		return;

	/* Calculate right and bottom deltas: */
	colorImage->getPixel(x + 1, y, color);
	float Lright = rgb2bw(color);
	colorImage->getPixel(x, y + 1, color);
	float Lbottom = rgb2bw(color);
	float Dright  = fabsf(L - Lright);
	float Dbottom = fabsf(L - Lbottom);

	/* Calculate the maximum delta in the direct neighborhood: */
	float maxDelta = fmaxf(fmaxf(Dleft, Dright), fmaxf(Dtop, Dbottom));

	/* Left edge */
	if (edges[0] != 0.0f) {
		/* Calculate left-left delta: */
		colorImage->getPixel(x - 2, y, color);
		float Lleftleft = rgb2bw(color);
		float Dleftleft = fabsf(Lleft - Lleftleft);

		/* Calculate the final maximum delta: */
		maxDelta = fmaxf(maxDelta, Dleftleft);

		/* Local contrast adaptation: */
		if (maxDelta > m_local_contrast_adaptation_factor * Dleft)
			edges[0] = 0.0f;
	}

	/* Top edge */
	if (edges[1] != 0.0f) {
		/* Calculate top-top delta: */
		colorImage->getPixel(x, y - 2, color);
		float Ltoptop = rgb2bw(color);
		float Dtoptop = fabsf(Ltop - Ltoptop);

		/* Calculate the final maximum delta: */
		maxDelta = fmaxf(maxDelta, Dtoptop);

		/* Local contrast adaptation: */
		if (maxDelta > m_local_contrast_adaptation_factor * Dtop)
			edges[1] = 0.0f;
	}
}

void PixelShader::getAreaLumaEdgeDetection(int *xmin, int *xmax, int *ymin, int *ymax)
{
	*xmin -= 2;
	*xmax += 1;
	*ymin -= 2;
	*ymax += 1;
}

/**
 * Color Edge Detection
 *
 * IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and
 * thus 'colorImage' should be a non-sRGB image.
 */
void PixelShader::colorEdgeDetection(int x, int y,
				     ImageReader *colorImage,
				     ImageReader *predicationImage,
				     /* out */ float edges[4])
{
	float threshold[2];

	/* Calculate the threshold: */
	if (m_enable_predication && predicationImage)
		calculatePredicatedThreshold(x, y, predicationImage, threshold);
	else
		threshold[0] = threshold[1] = m_threshold;

	/* Calculate color deltas: */
	float C[4], Cleft[4], Ctop[4];
	colorImage->getPixel(x, y, C);
	colorImage->getPixel(x - 1, y, Cleft);
	colorImage->getPixel(x, y - 1, Ctop);
	float Dleft = color_delta(C, Cleft);
	float Dtop  = color_delta(C, Ctop);

	/* We do the usual threshold: */
	edges[0] = step(threshold[0], Dleft);
	edges[1] = step(threshold[1], Dtop);
	edges[2] = 0.0f;
	edges[3] = 1.0f;

	/* Then discard if there is no edge: */
	if (edges[0] == 0.0f && edges[1] == 0.0f)
		return;

	/* Calculate right and bottom deltas: */
	float Cright[4], Cbottom[4];
	colorImage->getPixel(x + 1, y, Cright);
	colorImage->getPixel(x, y + 1, Cbottom);
	float Dright  = color_delta(C, Cright);
	float Dbottom = color_delta(C, Cbottom);

	/* Calculate the maximum delta in the direct neighborhood: */
	float maxDelta = fmaxf(fmaxf(Dleft, Dright), fmaxf(Dtop, Dbottom));

	/* Left edge */
	if (edges[0] != 0.0f) {
		/* Calculate left-left delta: */
		float Cleftleft[4];
		colorImage->getPixel(x - 2, y, Cleftleft);
		float Dleftleft = color_delta(Cleft, Cleftleft);

		/* Calculate the final maximum delta: */
		maxDelta = fmaxf(maxDelta, Dleftleft);

		/* Local contrast adaptation: */
		if (maxDelta > m_local_contrast_adaptation_factor * Dleft)
			edges[0] = 0.0f;
	}

	/* Top edge */
	if (edges[1] != 0.0f) {
		/* Calculate top-top delta: */
		float Ctoptop[4];
		colorImage->getPixel(x, y - 2, Ctoptop);
		float Dtoptop = color_delta(Ctop, Ctoptop);

		/* Calculate the final maximum delta: */
		maxDelta = fmaxf(maxDelta, Dtoptop);

		/* Local contrast adaptation: */
		if (maxDelta > m_local_contrast_adaptation_factor * Dtop)
			edges[1] = 0.0f;
	}
}

void PixelShader::getAreaColorEdgeDetection(int *xmin, int *xmax, int *ymin, int *ymax)
{
	*xmin -= 2;
	*xmax += 1;
	*ymin -= 2;
	*ymax += 1;
}

/**
 * Depth Edge Detection
 */
void PixelShader::depthEdgeDetection(int x, int y,
				     ImageReader *depthImage,
				     /* out */ float edges[4])
{
	float here[4], left[4], top[4];

	depthImage->getPixel(x, y, here);
	depthImage->getPixel(x - 1, y, left);
	depthImage->getPixel(x, y - 1, top);

	edges[0] = step(m_depth_threshold, fabsf(here[0] - left[0]));
	edges[1] = step(m_depth_threshold, fabsf(here[0] - top[0]));
	edges[2] = 0.0f;
	edges[3] = 1.0f;
}

void PixelShader::getAreaDepthEdgeDetection(int *xmin, int *xmax, int *ymin, int *ymax)
{
	*xmin -= 1;
	*ymin -= 1;
}

/*-----------------------------------------------------------------------------*/
/* Diagonal Search Functions */

/*
 * Note: Edges around a pixel (x, y)
 *
 *  - west  (left)  : R in (x, y)
 *  - north (top)   : G in (x, y)
 *  - east  (right) : R in (x + 1, y)
 *  - south (bottom): G in (x, y +1)
 */

/**
 * These functions allows to perform diagonal pattern searches.
 */
int PixelShader::searchDiag1(ImageReader *edgesImage, int x, int y, int dir,
			     /* out */ bool *found)
{
	float edges[4];
	int end = x + m_max_search_steps_diag * dir;
	*found = false;

	while (x != end) {
		x += dir;
		y -= dir; /* Search in direction to bottom-left or top-right */
		edgesImage->getPixel(x, y, edges);
		if (edges[1] == 0.0f) { /* north */
			*found = true;
			break;
		}
		if (edges[0] == 0.0f) { /* west */
			*found = true;
			/* Ended with north edge if dy > 0 (i.e. dir < 0) */
			return (dir < 0) ? x : x - dir;
		}
	}

	return x - dir;
}

int PixelShader::searchDiag2(ImageReader *edgesImage, int x, int y, int dir,
			     /* out */ bool *found)
{
	float edges[4];
	int end = x + m_max_search_steps_diag * dir;
	*found = false;

	while (x != end) {
		x += dir;
		y += dir; /* Search in direction to top-left or bottom-right */
		edgesImage->getPixel(x, y, edges);
		if (edges[1] == 0.0f) { /* north */
			*found = true;
			break;
		}
		edgesImage->getPixel(x + 1, y, edges);
		if (edges[0] == 0.0f) { /* east */
			*found = true;
			/* Ended with north edge if dy > 0 (i.e. dir > 0) */
			return (dir > 0) ? x : x - dir;
		}
	}

	return x - dir;
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
void PixelShader::calculateDiagWeights(ImageReader *edgesImage, int x, int y, const float edges[2],
				       const int subsampleIndices[4],
				       /* out */ float weights[2])
{
	int d1, d2;
	bool found1, found2;
	float e[4], c[4];

	weights[0] = weights[1] = 0.0f;

	if (m_max_search_steps_diag <= 0)
		return;

	/* Search for the line ends: */
	/*
	 *                        |
	 *                     2--3
	 *                     |
	 *                  1--2
	 *                  |    d2
	 *               0--1
	 *               |
	 *            0==0   Start from both ends of (x, y)'s north edge
	 *            |xy
	 *         1--0
	 *   d1    |
	 *      2--1
	 *      |
	 *   3--2
	 *   |
	 *
	 */
	if (edges[0] > 0.0f) { /* west of (x, y) */
		d1 = x - searchDiag1(edgesImage, x, y, -1, &found1);
	}
	else {
		d1 = 0;
		found1 = true;
	}
	d2 = searchDiag1(edgesImage, x, y, 1, &found2) - x;

	if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
		/* Fetch the crossing edges: */
		int e1 = 0, e2 = 0;
		/* e1, e2
		 *  0: none
		 *  1: vertical   (e1: down, e2: up)
		 *  2: horizontal (e1: left, e2: right)
		 *  3: both
		 *
		 * Possible depending area:
		 *  max distances are: d1=N, d2=N-1
		 *  x range [x-N-1, x+(N-1)+1] = [x-N-1, x+N] ... (1)
		 *  y range [y-(N-1)-1, y+N]   = [y-N,   y+N] ... (2)
		 *
		 * where N is max search distance
		 */
		if (found1) {
			int co_x = x - d1, co_y = y + d1;
			edgesImage->getPixel(co_x - 1, co_y, c);
			if (c[1] > 0.0f)
				e1 += 2; /* ...->left->left */
			edgesImage->getPixel(co_x, co_y, c);
			if (c[0] > 0.0f)
				e1 += 1; /* ...->left->down->down */
		}
		if (found2) {
			int co_x = x + d2, co_y = y - d2;
			edgesImage->getPixel(co_x + 1, co_y, c);
			if (c[1] > 0.0f)
				e2 += 2; /* ...->right->right */
			edgesImage->getPixel(co_x + 1, co_y - 1, c);
			if (c[0] > 0.0f)
				e2 += 1; /* ...->right->up->up */
		}

		/* Fetch the areas for this line: */
		area_diag(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[2] : 0), weights);
	}

	/* Search for the line ends: */
	/*
	 *   |
	 *   3--2
	 *      |
	 *      2--1
	 *   d1    |
	 *         1--0
	 *            |
	 *            0==0   Start from both ends of (x, y)'s north edge
	 *             xy|
	 *               0--1
	 *                  |    d2
	 *                  1--2
	 *                     |
	 *                     2--3
	 *                        |
	 *
	 */
	d1 = x - searchDiag2(edgesImage, x, y, -1, &found1);
	edgesImage->getPixel(x + 1, y, e);
	if (e[0] > 0.0f) { /* east of (x, y) */
		d2 = searchDiag2(edgesImage, x, y, 1, &found2) - x;
	}
	else {
		d2 = 0;
		found2 = true;
	}

	if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
		/* Fetch the crossing edges: */
		int e1 = 0, e2 = 0;
		/* e1, e2
		 *  0: none
		 *  1: vertical   (e1: up, e2: down)
		 *  2: horizontal (e1: left, e2: right)
		 *  3: both
		 *
		 * Possible depending area:
		 *  max distances are: d1=N-1, d2=N
		 *  x range [x-(N-1)-1, x+N+1] = [x-N, x+N+1] ... (3)
		 *  y range [y-(N-1)-1, y+N]   = [y-N, y+N]   ... (4)
		 *
		 * where N is max search distance
		 */
		if (found1) {
			int co_x = x - d1, co_y = y - d1;
			edgesImage->getPixel(co_x - 1, co_y, c);
			if (c[1] > 0.0f)
				e1 += 2; /* ...->left->left */
			edgesImage->getPixel(co_x, co_y - 1, c);
			if (c[0] > 0.0f)
				e1 += 1; /* ...->left->up->up */
		}
		if (found2) {
			int co_x = x + d2, co_y = y + d2;
			edgesImage->getPixel(co_x + 1, co_y, c);
			if (c[1] > 0.0f)
				e2 += 2; /* ...->right->right */
			if (c[0] > 0.0f)
				e2 += 1; /* ...->right->down->down */
		}

		/* Fetch the areas for this line: */
		float w[2];
		area_diag(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[3] : 0), w);
		weights[0] += w[1];
		weights[1] += w[0];
	}
}

bool PixelShader::isVerticalSearchUnneeded(ImageReader *edgesImage, int x, int y)
{
	int d1, d2;
	bool found;
	float e[4];

	if (m_max_search_steps_diag <= 0)
		return false;

	/* Search for the line ends: */
	/*
	 *   |
	 *   3--2
	 *      |
	 *      2--1
	 *   d1    |
	 *         1--0
	 *            |
	 *            0==0   Start from both ends of (x-1, y)'s north edge
	 *               |xy
	 *               0--1
	 *                  |    d2
	 *                  1--2
	 *                     |
	 *                     2--3
	 *                        |
	 *
	 * We've already done diagonal search and weight calculation in this direction,
	 * so want to know only whether there is diagonal edge in order to avoid
	 * performing unneeded vertical search and weight calculations.
	 */
	edgesImage->getPixel(x - 1, y, e);
	if (e[1] > 0.0f) /* north of (x-1, y) */
		d1 = x - searchDiag2(edgesImage, x - 1, y, -1, &found);
	else
		d1 = 0;
	d2 = searchDiag2(edgesImage, x - 1, y, 1, &found) - x;
	/*
	 * Possible depending area:
	 *  x range [(x-1)-(N-1), (x-1)+(N-1)+1] = [x-N,   x+N-1] ... (5)
	 *  y range [y-(N-1), y+(N-1)]           = [y-N+1, y+N-1] ... (6)
	 *
	 * where N is max search distance
	 */

	return (d1 + d2 > 2); /* d1 + d2 + 1 > 3 */
}

/*
 * Final depending area considering all diagonal searches:
 *  x range: (1)(3)(5) -> [x-N-1, x+N+1] = [x-(N+1), x+(N+1)]
 *  y range: (2)(4)(6) -> [y-N,   y+N]
 */

/*-----------------------------------------------------------------------------*/
/* Horizontal/Vertical Search Functions */

int PixelShader::searchXLeft(ImageReader *edgesImage, int x, int y)
{
	int end = x - m_max_search_steps;
	float edges[4];

	while (x > end) {
		edgesImage->getPixel(x, y, edges);
		if (edges[1] == 0.0f)   /* Is the north edge not activated? */
			break; /* x + 1 */
		if (edges[0] != 0.0f)   /* Or is there a bottom crossing edge that breaks the line? */
			return x;
		edgesImage->getPixel(x, y - 1, edges);
		if (edges[0] != 0.0f)   /* Or is there a top crossing edge that breaks the line? */
			return x;
		x--;
	}

	return x + 1;
}

int PixelShader::searchXRight(ImageReader *edgesImage, int x, int y)
{
	int end = x + m_max_search_steps;
	float edges[4];

	while (x < end) {
		x++;
		edgesImage->getPixel(x, y, edges);
		if (edges[1] == 0.0f || /* Is the north edge not activated? */
		    edges[0] != 0.0f)   /* Or is there a bottom crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x, y - 1, edges);
		if (edges[0] != 0.0f)   /* Or is there a top crossing edge that breaks the line? */
			break;
	}

	return x - 1;
}

int PixelShader::searchYUp(ImageReader *edgesImage, int x, int y)
{
	int end = y - m_max_search_steps;
	float edges[4];

	while (y > end) {
		edgesImage->getPixel(x, y, edges);
		if (edges[0] == 0.0f)   /* Is the west edge not activated? */
			break; /* y + 1 */
		if (edges[1] != 0.0f)   /* Or is there a right crossing edge that breaks the line? */
			return y;
		edgesImage->getPixel(x - 1, y, edges);
		if (edges[1] != 0.0f)   /* Or is there a left crossing edge that breaks the line? */
			return y;
		y--;
	}

	return y + 1;
}

int PixelShader::searchYDown(ImageReader *edgesImage, int x, int y)
{
	int end = y + m_max_search_steps;
	float edges[4];

	while (y < end) {
		y++;
		edgesImage->getPixel(x, y, edges);
		if (edges[0] == 0.0f || /* Is the west edge not activated? */
		    edges[1] != 0.0f)   /* Or is there a right crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x - 1, y, edges);
		if (edges[1] != 0.0f)   /* Or is there a left crossing edge that breaks the line? */
			break;
	}

	return y - 1;
}

/*-----------------------------------------------------------------------------*/
/*  Corner Detection Functions */

void PixelShader::detectHorizontalCornerPattern(ImageReader *edgesImage,
						/* inout */ float weights[4],
						int left, int right, int y, int d1, int d2)
{
	float factor[2] = {1.0f, 1.0f};
	float rounding = 1.0f - (float)m_corner_rounding / 100.0f;
	float edges[4];

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d1 == d2) ? 0.5f : 1.0f;

	/* Near the left corner */
	if (d1 <= d2) {
		edgesImage->getPixel(left, y + 1, edges);
		factor[0] -= rounding * edges[0];
		edgesImage->getPixel(left, y - 2, edges);
		factor[1] -= rounding * edges[0];
	}
	/* Near the right corner */
	if (d1 >= d2) {
		edgesImage->getPixel(right + 1, y + 1, edges);
		factor[0] -= rounding * edges[0];
		edgesImage->getPixel(right + 1, y - 2, edges);
		factor[1] -= rounding * edges[0];
	}

	weights[0] *= saturate(factor[0]);
	weights[1] *= saturate(factor[1]);
}

void PixelShader::detectVerticalCornerPattern(ImageReader *edgesImage,
					      /* inout */ float weights[4],
					      int top, int bottom, int x, int d1, int d2)
{
	float factor[2] = {1.0f, 1.0f};
	float rounding = 1.0f - (float)m_corner_rounding / 100.0f;
	float edges[4];

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d1 == d2) ? 0.5f : 1.0f;

	/* Near the top corner */
	if (d1 <= d2) {
		edgesImage->getPixel(x + 1, top, edges);
		factor[0] -= rounding * edges[1];
		edgesImage->getPixel(x - 2, top, edges);
		factor[1] -= rounding * edges[1];
	}
	/* Near the bottom corner */
	if (d1 >= d2) {
		edgesImage->getPixel(x + 1, bottom + 1, edges);
		factor[0] -= rounding * edges[1];
		edgesImage->getPixel(x - 2, bottom + 1, edges);
		factor[1] -= rounding * edges[1];
	}

	weights[2] *= saturate(factor[0]);
	weights[3] *= saturate(factor[1]);
}

/*-----------------------------------------------------------------------------*/
/* Blending Weight Calculation Pixel Shader (Second Pass) */
/*   Just pass zero to subsampleIndices for SMAA 1x, see @SUBSAMPLE_INDICES. */

void PixelShader::blendingWeightCalculation(int x, int y,
					    ImageReader *edgesImage,
					    const int subsampleIndices[4],
					    /* out */ float weights[4])
{
	float edges[4], c[4];

	weights[0] = weights[1] = weights[2] = weights[3] = 0.0f;
	edgesImage->getPixel(x, y, edges);

	if (edges[1] > 0.0f) { /* Edge at north */
		if (m_enable_diag_detection) {
			/* Diagonals have both north and west edges, so calculating weights for them */
			/* in one of the boundaries is enough. */
			calculateDiagWeights(edgesImage, x, y, edges, subsampleIndices, weights);

			/* We give priority to diagonals, so if we find a diagonal we skip  */
			/* horizontal/vertical processing. */
			if (weights[0] + weights[1] != 0.0f)
				return;
		}

		/* Find the distance to the left and the right: */
		/*
		 *   <- left  right ->
		 *   2  1  0  0  1  2
		 *   |  |  |  |  |  |
		 * --2--1--0==0--1--2--
		 *   |  |  |xy|  |  |
		 *   2  1  0  0  1  2
		 */
		int left = searchXLeft(edgesImage, x, y);
		int right = searchXRight(edgesImage, x, y);
		int d1 = x - left, d2 = right - x;

		/* Now fetch the left and right crossing edges: */
		int e1 = 0, e2 = 0;
		/* e1, e2
		 *  0: none
		 *  1: top
		 *  2: bottom
		 *  3: both
		 *
		 * Possible depending area:
		 *  max distances are: d1=N-1, d2=N-1
		 *  x range [x-(N-1), x+(N-1)+1] = [x-N+1, x+N] ... (1)
		 *  y range [y-1, y] ... (2)
		 *
		 * where N is max search distance
		 */
		edgesImage->getPixel(left, y - 1, c);
		if (c[0] > 0.0f)
			e1 += 1;
		edgesImage->getPixel(left, y, c);
		if (c[0] > 0.0f)
			e1 += 2;
		edgesImage->getPixel(right + 1, y - 1, c);
		if (c[0] > 0.0f)
			e2 += 1;
		edgesImage->getPixel(right + 1, y, c);
		if (c[0] > 0.0f)
			e2 += 2;

		/* Ok, we know how this pattern looks like, now it is time for getting */
		/* the actual area: */
		area(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[1] : 0), weights);

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectHorizontalCornerPattern(edgesImage, weights, left, right, y, d1, d2);
	}

	if (edges[0] > 0.0f) { /* Edge at west */
		/* Did we already do diagonal search for this west edge from the left neighboring pixel? */
		if (m_enable_diag_detection && isVerticalSearchUnneeded(edgesImage, x, y))
			return;

		/* Find the distance to the top and the bottom: */
		/*      |
		 *   2--2--2
		 *      |
		 *   1--1--1   A
		 *      |      |
		 *   0--0--0  top
		 *     ||xy
		 *   0--0--0 bottom
		 *      |      |
		 *   1--1--1   V
		 *      |
		 *   2--2--2
		 *      |      */
		int top = searchYUp(edgesImage, x, y);
		int bottom = searchYDown(edgesImage, x, y);
		int d1 = y - top, d2 = bottom - y;

		/* Fetch the top ang bottom crossing edges: */
		int e1 = 0, e2 = 0;
		/* e1, e2
		 *  0: none
		 *  1: left
		 *  2: right
		 *  3: both
		 *
		 * Possible depending area:
		 *  max distances are: d1=N-1, d2=N-1
		 *  x range [x-1, x] ... (3)
		 *  y range [y-(N-1), y+(N-1)+1] = [y-N+1, y+N] ... (4)
		 *
		 * where N is max search distance
		 */
		edgesImage->getPixel(x - 1, top, c);
		if (c[1] > 0.0f)
			e1 += 1;
		edgesImage->getPixel(x, top, c);
		if (c[1] > 0.0f)
			e1 += 2;
		edgesImage->getPixel(x - 1, bottom + 1, c);
		if (c[1] > 0.0f)
			e2 += 1;
		edgesImage->getPixel(x, bottom + 1, c);
		if (c[1] > 0.0f)
			e2 += 2;

		/* Get the area for this direction: */
		area(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[0] : 0), weights + 2);

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectVerticalCornerPattern(edgesImage, weights, top, bottom, x, d1, d2);
	}
	/*
	 * Final depending area considering all orthogonal searches:
	 *  x range: (1),(3) -> [min(x-N+1, x-1), x+N] = [x-max(N-1, 1), x+N]
	 *  y range: (2),(4) -> [min(x-N+1, y-1), y+N] = [y-max(N-1, 1), y+N]
	 */
}

void PixelShader::getAreaBlendingWeightCalculation(int *xmin, int *xmax, int *ymin, int *ymax)
{
	using std::max;

	*xmin -= max(max(m_max_search_steps - 1, 1),
		     m_enable_diag_detection ? m_max_search_steps_diag + 1 : 0);
	*xmax += max(m_max_search_steps,
		     m_enable_diag_detection ? m_max_search_steps_diag + 1 : 0);
	*ymin -= max(max(m_max_search_steps - 1, 1),
		     m_enable_diag_detection ? m_max_search_steps_diag : 0);
	*ymax += max(m_max_search_steps,
		     m_enable_diag_detection ? m_max_search_steps_diag : 0);
}

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending Pixel Shader (Third Pass) */

void PixelShader::neighborhoodBlending(int x, int y,
				       ImageReader *colorImage,
				       ImageReader *blendImage,
				       ImageReader *velocityImage,
				       /* out */ float color[4])
{
	float w[4];

	/* Fetch the blending weights for current pixel: */
	blendImage->getPixel(x, y, w);
	float left = w[2], top = w[0];
	blendImage->getPixel(x + 1, y , w);
	float right = w[3];
	blendImage->getPixel(x, y + 1, w);
	float bottom = w[1];

	/* Is there any blending weight with a value greater than 0.0? */
	if (right + bottom + left + top < 1e-5) {
		colorImage->getPixel(x, y, color);

		if (m_enable_reprojection && velocityImage) {
			float velocity[4];
			velocityImage->getPixel(x, y, velocity);

			/* Pack velocity into the alpha channel: */
			color[3] = sqrtf(5.0f * sqrtf(velocity[0] * velocity[0] + velocity[1] * velocity[1]));
		}

		return;
	}

	/* Calculate the blending offsets: */
	void (*samplefunc)(ImageReader *image, int x, int y, float offset, float color[4]);
	float offset1, offset2, weight1, weight2;

	if (fmaxf(right, left) > fmaxf(bottom, top)) { /* max(horizontal) > max(vertical) */
		samplefunc = sample_bilinear_horizontal;
		offset1 = right;
		offset2 = -left;
		weight1 = right / (right + left);
		weight2 = left / (right + left);
	}
	else {
		samplefunc = sample_bilinear_vertical;
		offset1 = bottom;
		offset2 = -top;
		weight1 = bottom / (bottom + top);
		weight2 = top / (bottom + top);
	}

	/* We exploit bilinear filtering to mix current pixel with the chosen neighbor: */
	float color1[4], color2[4];
	samplefunc(colorImage, x, y, offset1, color1);
	samplefunc(colorImage, x, y, offset2, color2);

	color[0] = weight1 * color1[0] + weight2 * color2[0];
	color[1] = weight1 * color1[1] + weight2 * color2[1];
	color[2] = weight1 * color1[2] + weight2 * color2[2];
	color[3] = weight1 * color1[3] + weight2 * color2[3];

	if (m_enable_reprojection && velocityImage) {
		/* Antialias velocity for proper reprojection in a later stage: */
		float velocity1[4], velocity2[4];
		samplefunc(velocityImage, x, y, offset1, velocity1);
		samplefunc(velocityImage, x, y, offset2, velocity2);
		float velocity_x = weight1 * velocity1[0] + weight2 * velocity2[0];
		float velocity_y = weight1 * velocity1[1] + weight2 * velocity2[1];

		/* Pack velocity into the alpha channel: */
		color[3] = sqrtf(5.0f * sqrtf(velocity_x * velocity_x + velocity_y * velocity_y));
	}
}

void PixelShader::getAreaNeighborhoodBlending(int *xmin, int *xmax, int *ymin, int *ymax)
{
	*xmin -= 1;
	*xmax += 1;
	*ymin -= 1;
	*ymax += 1;
}

/*-----------------------------------------------------------------------------*/
/* Temporal Resolve Pixel Shader (Optional Pass) -- untested yet! */

void PixelShader::resolve(int x, int y,
			  ImageReader *currentColorImage,
			  ImageReader *previousColorImage,
			  ImageReader *velocityImage,
			  /* out */ float color[4])
{
	if (m_enable_predication && velocityImage) {
		/* Velocity is assumed to be calculated for motion blur, so we need to */
		/* inverse it for reprojection: */
		float velocity[4];
		velocityImage->getPixel(x, y, velocity);

		/* Fetch current pixel: */
		float current[4];
		currentColorImage->getPixel(x, y, current);

		/* Reproject current coordinates and fetch previous pixel: */
		float previous[4];
		sample_bilinear(previousColorImage, x - velocity[0], y - velocity[1], previous);

		/* Attenuate the previous pixel if the velocity is different: */
		float delta = fabsf(current[3] * current[3] - previous[3] * previous[3]) / 5.0f;
		float weight = 0.5f * saturate(1.0f - sqrtf(delta) * m_reprojection_weight_scale);

		/* Blend the pixels according to the calculated weight: */
		color[0] = lerp(current[0], previous[0], weight);
		color[1] = lerp(current[1], previous[1], weight);
		color[2] = lerp(current[2], previous[2], weight);
		color[3] = lerp(current[3], previous[3], weight);
	}
	else {
		/* Just blend the pixels: */
		float current[4], previous[4];
		currentColorImage->getPixel(x, y, current);
		previousColorImage->getPixel(x, y, previous);
		color[0] = (current[0] + previous[0]) * 0.5f;
		color[1] = (current[1] + previous[1]) * 0.5f;
		color[2] = (current[2] + previous[2]) * 0.5f;
		color[3] = (current[3] + previous[3]) * 0.5f;
	}
}

/*-----------------------------------------------------------------------------*/

}
/* smaa.cpp ends here */
