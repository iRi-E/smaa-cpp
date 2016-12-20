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
#include "smaa.h"
#include "smaa_areatex.h"

namespace SMAA {

/*-----------------------------------------------------------------------------*/
/* Non-Configurable Defines */

static const int SMAA_AREATEX_SIZE = 80; /* 16 * 5 = 20 * 4 = 80 */
static const int SMAA_AREATEX_MAX_DISTANCE = 16;
static const int SMAA_AREATEX_MAX_DISTANCE_DIAG = 20;
static const float RGB_WEIGHTS[3] = {0.2126, 0.7152, 0.0722};

/*-----------------------------------------------------------------------------*/
/* Misc functions */

static float step(float edge, float x)
{
	return x < edge ? 0.0 : 1.0;
}

static float saturate(float x)
{
	return 0.0 < x ? (x < 1.0 ? x : 1.0) : 0.0;
}

static int clamp(int x, int range)
{
	return 0 < x ? (x < range ? x : range - 1) : 0;
}

static float lerp(float a, float b, float p)
{
	return a + (b - a) * p;
}

static float bilinear(float c00, float c10, float c01, float c11, float x, float y)
{
	return (c00 * (1.0 - x) + c10 * x) * (1.0 - y) + (c01 * (1.0 - x) + c11 * x) * y;
}

static float rgb2bw(const float color[4])
{
	return RGB_WEIGHTS[0] * color[0] + RGB_WEIGHTS[1] * color[1] + RGB_WEIGHTS[2] * color[2];
}

static float color_delta(const float color1[4], const float color2[4])
{
	return fmaxf(fmaxf(fabsf(color1[0] - color2[0]), fabsf(color1[1] - color2[1])), fabsf(color1[2] - color2[2]));
}

static void sample(ImageReader *image, float x, float y, float output[4])
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

static void sampleOffsetVertical(ImageReader *image, int x, int y, float yoffset, float output[4])
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

static void sampleOffsetHorizontal(ImageReader *image, int x, int y, float xoffset, float output[4])
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

static const float* areatex_sample_internal(const float *areatex, int x, int y)
{
	return &areatex[(clamp(x, SMAA_AREATEX_SIZE) +
			 clamp(y, SMAA_AREATEX_SIZE) * SMAA_AREATEX_SIZE) * 2];
}

static void areaTexSampleLevelZero(const float *areatex, float x, float y, float weights[2])
{
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

	threshold[0] = scaled * (1.0 - m_predication_strength * edges[0]);
	threshold[1] = scaled * (1.0 - m_predication_strength * edges[1]);
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
	edges[2] = 0.0;
	edges[3] = 1.0;

	/* Then discard if there is no edge: */
	if (edges[0] == 0.0 && edges[1] == 0.0)
		return;

	/* Calculate right and bottom deltas: */
	colorImage->getPixel(x + 1, y, color);
	float Lright = rgb2bw(color);
	colorImage->getPixel(x, y + 1, color);
	float Lbottom = rgb2bw(color);
	float Dright  = fabsf(L - Lright);
	float Dbottom = fabsf(L - Lbottom);

	/* Calculate left-left and top-top deltas: */
	colorImage->getPixel(x - 2, y, color);
	float Lleftleft = rgb2bw(color);
	colorImage->getPixel(x, y - 2, color);
	float Ltoptop = rgb2bw(color);
	float Dleftleft = fabsf(Lleft - Lleftleft);
	float Dtoptop   = fabsf(Ltop - Ltoptop);

	/* Calculate the maximum delta: */
	float maxDelta_x = fmaxf(fmaxf(Dleft, Dright), Dleftleft);
	float maxDelta_y = fmaxf(fmaxf(Dtop, Dbottom), Dtoptop);
	float finalDelta = fmaxf(maxDelta_x, maxDelta_y);

	/* Local contrast adaptation: */
	if (finalDelta > m_local_contrast_adaptation_factor * Dleft)
		edges[0] = 0.0;
	if (finalDelta > m_local_contrast_adaptation_factor * Dtop)
		edges[1] = 0.0;
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
	edges[2] = 0.0;
	edges[3] = 1.0;

	/* Then discard if there is no edge: */
	if (edges[0] == 0.0 && edges[1] == 0.0)
		return;

	/* Calculate right and bottom deltas: */
	float Cright[4], Cbottom[4];
	colorImage->getPixel(x + 1, y, Cright);
	colorImage->getPixel(x, y + 1, Cbottom);
	float Dright  = color_delta(C, Cright);
	float Dbottom = color_delta(C, Cbottom);

	/* Calculate left-left and top-top deltas: */
	float Cleftleft[4], Ctoptop[4];
	colorImage->getPixel(x - 2, y, Cleftleft);
	colorImage->getPixel(x, y - 2, Ctoptop);
	float Dleftleft = color_delta(Cleft, Cleftleft);
	float Dtoptop   = color_delta(Ctop, Ctoptop);

	/* Calculate the maximum delta: */
	float maxDelta_x = fmaxf(fmaxf(Dleft, Dright), Dleftleft);
	float maxDelta_y = fmaxf(fmaxf(Dtop, Dbottom), Dtoptop);
	float finalDelta = fmaxf(maxDelta_x, maxDelta_y);

	/* Local contrast adaptation: */
	if (finalDelta > m_local_contrast_adaptation_factor * Dleft)
		edges[0] = 0.0;
	if (finalDelta > m_local_contrast_adaptation_factor * Dtop)
		edges[1] = 0.0;
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
	edges[2] = 0.0;
	edges[3] = 1.0;
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
int PixelShader::searchDiag1(ImageReader *edgesImage, int x, int y, int dx, int dy,
			     /* out */ float *end, bool *found)
{
	float edges[4];
	int dist = -1;
	*found = false;

	while (dist < (m_max_search_steps_diag - 1)) {
		x += dx;
		y += dy;
		dist++;
		edgesImage->getPixel(x, y, edges); /* west & north */
		if (!(edges[0] > 0.9 && edges[1] > 0.9)) {
			*found = true;
			break;
		}
	}

	*end = edges[1]; /* return north */
	return dist;
}

int PixelShader::searchDiag2(ImageReader *edgesImage, int x, int y, int dx, int dy,
			     /* out */ float *end, bool *found)
{
	float edges1[4], edges2[4];
	int dist = -1;
	*found = false;

	while (dist < (m_max_search_steps_diag - 1)) {
		x += dx;
		y += dy;
		dist++;
		edgesImage->getPixel(x + 1, y, edges1); /* east */
		edgesImage->getPixel(x, y, edges2);     /* north */
		if (!(edges1[0] > 0.9 && edges2[1] > 0.9)) {
			*found = true;
			break;
		}
	}

	*end = edges2[1]; /* return north */
	return dist;
}

/**
 * Similar to area(), this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
static void areaDiag(int d1, int d2, int e1, int e2, float offset,
		     /* out */ float weights[2])
{
	float x = (float)(SMAA_AREATEX_MAX_DISTANCE_DIAG * e1 + d1);
	float y = (float)(SMAA_AREATEX_MAX_DISTANCE_DIAG * e2 + d2);

	/* We do a bias for mapping to texel space: */
	x += 0.5;
	y += 0.5;

	/* Move to proper place, according to the subpixel offset: */
	y += (float)SMAA_AREATEX_SIZE * offset;

	/* Do it! */
	areaTexSampleLevelZero(areatex_diag, x, y, weights);
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
void PixelShader::calculateDiagWeights(ImageReader *edgesImage, int x, int y, float e[2],
				       const float subsampleIndices[4],
				       /* out */ float weights[2])
{
	int d1, d2;
	bool found1, found2;
	float end, edges[4];

	weights[0] = weights[1] = 0.0;

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
	if (e[0] > 0.0) {
		d1 = searchDiag1(edgesImage, x, y, -1, 1, &end, &found1);
		/* Ended with north edge? */
		if (end > 0.0)
			d1++;
	}
	else {
		d1 = 0;
		found1 = true;
	}
	d2 = searchDiag1(edgesImage, x, y, 1, -1, &end, &found2);

	if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
		/* Fetch the crossing edges: */
		int e1 = 0, e2 = 0;
		/* e1, e2
		 *  0: none
		 *  1: vertical   (e1: down, e2: up)
		 *  2: horizontal (e1: left, e2: right)
		 *  3: both
		 */
		if (found1) {
			int co_x = x - d1, co_y = y + d1;
			edgesImage->getPixel(co_x - 1, co_y, edges);
			if (edges[1] > 0.0)
				e1 += 2; /* ...->left->left */
			edgesImage->getPixel(co_x, co_y, edges);
			if (edges[0] > 0.0)
				e1 += 1; /* ...->left->down->down */
		}
		if (found2) {
			int co_x = x + d2, co_y = y - d2;
			edgesImage->getPixel(co_x + 1, co_y, edges);
			if (edges[1] > 0.0)
				e2 += 2; /* ...->right->right */
			edgesImage->getPixel(co_x + 1, co_y - 1, edges);
			if (edges[0] > 0.0)
				e2 += 1; /* ...->right->up->up */
		}

		/* Fetch the areas for this line: */
		areaDiag(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[2] : 0.0), weights);
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
	d1 = searchDiag2(edgesImage, x, y, -1, -1, &end, &found1);
	edgesImage->getPixel(x + 1, y, edges);
	if (edges[0] > 0.0) {
		d2 = searchDiag2(edgesImage, x, y, 1, 1, &end, &found2);
		/* Ended with north edge? */
		if (end > 0.0)
			d2++;
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
		 */
		if (found1) {
			int co_x = x - d1, co_y = y - d1;
			edgesImage->getPixel(co_x - 1, co_y, edges);
			if (edges[1] > 0.0)
				e1 += 2; /* ...->left->left */
			edgesImage->getPixel(co_x, co_y - 1, edges);
			if (edges[0] > 0.0)
				e1 += 1; /* ...->left->up->up */
		}
		if (found2) {
			int co_x = x + d2, co_y = y + d2;
			edgesImage->getPixel(co_x + 1, co_y, edges);
			if (edges[1] > 0.0)
				e2 += 2; /* ...->right->right */
			if (edges[0] > 0.0)
				e2 += 1; /* ...->right->down->down */
		}

		/* Fetch the areas for this line: */
		float w[2];
		areaDiag(d1, d2, e1, e2, (subsampleIndices ? subsampleIndices[3] : 0.0), w);
		weights[0] += w[1];
		weights[1] += w[0];
	}
}

/*-----------------------------------------------------------------------------*/
/* Horizontal/Vertical Search Functions */

int PixelShader::searchXLeft(ImageReader *edgesImage, int x, int y)
{
	int end = x - 2 * m_max_search_steps - 1;
	float edges[4];

	while (x >= end) {
		edgesImage->getPixel(x, y, edges);
		if (edges[1] == 0.0 || /* Is the edge not activated? */
		    edges[0] != 0.0)   /* Or is there a bottom crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x, y - 1, edges);
		if (edges[0] != 0.0)   /* Or is there a top crossing edge that breaks the line? */
			break;
		x--;
	}

	return x;
}

int PixelShader::searchXRight(ImageReader *edgesImage, int x, int y)
{
	int end = x + 2 * m_max_search_steps + 1;
	float edges[4];

	while (x <= end) {
		edgesImage->getPixel(x + 1, y, edges);
		if (edges[1] == 0.0 || /* Is the edge not activated? */
		    edges[0] != 0.0)   /* Or is there a bottom crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x + 1, y - 1, edges);
		if (edges[0] != 0.0)   /* Or is there a top crossing edge that breaks the line? */
			break;
		x++;
	}

	return x;
}

int PixelShader::searchYUp(ImageReader *edgesImage, int x, int y)
{
	int end = y - 2 * m_max_search_steps - 1;
	float edges[4];

	while (y >= end) {
		edgesImage->getPixel(x, y, edges);
		if (edges[0] == 0.0 || /* Is the edge not activated? */
		    edges[1] != 0.0)   /* Or is there a right crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x - 1, y, edges);
		if (edges[1] != 0.0)   /* Or is there a left crossing edge that breaks the line? */
			break;
		y--;
	}

	return y;
}

int PixelShader::searchYDown(ImageReader *edgesImage, int x, int y)
{
	int end = y + 2 * m_max_search_steps + 1;
	float edges[4];

	while (y <= end) {
		edgesImage->getPixel(x, y + 1, edges);
		if (edges[0] == 0.0 || /* Is the edge not activated? */
		    edges[1] != 0.0)   /* Or is there a right crossing edge that breaks the line? */
			break;
		edgesImage->getPixel(x - 1, y + 1, edges);
		if (edges[1] != 0.0)   /* Or is there a left crossing edge that breaks the line? */
			break;
		y++;
	}

	return y;
}

/**
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
static void area(float sqrt_d1, float sqrt_d2, int e1, int e2, float offset,
		 /* out */ float weights[2])
{
	/* Rounding prevents precision errors of bilinear filtering: */
	float x = (float)(SMAA_AREATEX_MAX_DISTANCE * e1) + sqrt_d1;
	float y = (float)(SMAA_AREATEX_MAX_DISTANCE * e2) + sqrt_d2;

	/* We do a bias for mapping to texel space: */
	x += 0.5;
	y += 0.5;

	/* Move to proper place, according to the subpixel offset: */
	y += (float)SMAA_AREATEX_SIZE * offset;

	/* Do it! */
	areaTexSampleLevelZero(areatex, x, y, weights);
}

/*-----------------------------------------------------------------------------*/
/*  Corner Detection Functions */

void PixelShader::detectHorizontalCornerPattern(ImageReader *edgesImage,
						/* inout */ float weights[4],
						int left, int right, int y, int d1, int d2)
{
	float factor[2] = {1.0, 1.0};
	float rounding = 1.0 - (float)m_corner_rounding / 100.0;
	float edges[4];

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d1 == d2) ? 0.5 : 1.0;

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
	float factor[2] = {1.0, 1.0};
	float rounding = 1.0 - (float)m_corner_rounding / 100.0;
	float edges[4];

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d1 == d2) ? 0.5 : 1.0;

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
					    const float subsampleIndices[4],
					    /* out */ float weights[4])
{
	float w[2], edges[4];

	weights[0] = weights[1] = weights[2] = weights[3] = 0.0;
	edgesImage->getPixel(x, y, edges);

	if (edges[1] > 0.0) { /* Edge at north */
		if (m_enable_diag_detection) {
			/* Diagonals have both north and west edges, so searching for them in */
			/* one of the boundaries is enough. */
			calculateDiagWeights(edgesImage, x, y, edges, subsampleIndices, w);

			/* We give priority to diagonals, so if we find a diagonal we skip  */
			/* horizontal/vertical processing. */
			if (w[0] + w[1] != 0.0) {
				weights[0] = w[0];
				weights[1] = w[1];
				return;
			}
		}

		/* Find the distance to the left and the right: */
		int left = searchXLeft(edgesImage, x, y);
		int right = searchXRight(edgesImage, x, y);
		int d1 = abs(left - x), d2 = abs(right - x);

		/* Now fetch the left and right crossing edges, two at a time using bilinear */
		/* filtering. Sampling at -0.25 enables to discern what value each edge has: */
		float edges[4];
		int e1 = 0, e2 = 0;
		edgesImage->getPixel(left, y - 1, edges);
		if (edges[0] > 0.0)
			e1 += 1;
		edgesImage->getPixel(left, y, edges);
		if (edges[0] > 0.0)
			e1 += 3;
		edgesImage->getPixel(right + 1, y - 1, edges);
		if (edges[0] > 0.0)
			e2 += 1;
		edgesImage->getPixel(right + 1, y, edges);
		if (edges[0] > 0.0)
			e2 += 3;

		/* area() below needs a sqrt, as the areas texture is compressed */
		/* quadratically: */
		float sqrt_d1 = sqrtf((float)d1), sqrt_d2 = sqrtf((float)d2);

		/* Ok, we know how this pattern looks like, now it is time for getting */
		/* the actual area: */
		area(sqrt_d1, sqrt_d2, e1, e2, (subsampleIndices ? subsampleIndices[1] : 0.0), w);
		weights[0] = w[0];
		weights[1] = w[1];

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectHorizontalCornerPattern(edgesImage, weights, left, right, y, d1, d2);
	}

	if (edges[0] > 0.0) { /* Edge at west */

		/* Find the distance to the top and the bottom: */
		int top = searchYUp(edgesImage, x, y);
		int bottom = searchYDown(edgesImage, x, y);
		int d1 = abs(top - y), d2 = abs(bottom - y);

		/* Fetch the top ang bottom crossing edges: */
		float edges[4];
		int e1 = 0, e2 = 0;
		edgesImage->getPixel(x - 1, top, edges);
		if (edges[1] > 0.0)
			e1 += 1;
		edgesImage->getPixel(x, top, edges);
		if (edges[1] > 0.0)
			e1 += 3;
		edgesImage->getPixel(x - 1, bottom + 1, edges);
		if (edges[1] > 0.0)
			e2 += 1;
		edgesImage->getPixel(x, bottom + 1, edges);
		if (edges[1] > 0.0)
			e2 += 3;

		/* area() below needs a sqrt, as the areas texture is compressed  */
		/* quadratically: */
		float sqrt_d1 = sqrtf((float)d1), sqrt_d2 = sqrtf((float)d2);

		/* Get the area for this direction: */
		area(sqrt_d1, sqrt_d2, e1, e2, (subsampleIndices ? subsampleIndices[0] : 0.0), w);
		weights[2] = w[0];
		weights[3] = w[1];

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectVerticalCornerPattern(edgesImage, weights, top, bottom, x, d1, d2);
	}
}

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending Pixel Shader (Third Pass) */

void PixelShader::neighborhoodBlending(int x, int y,
				       ImageReader *colorImage,
				       ImageReader *blendImage,
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
		return;
	}

	/* Calculate the blending offsets: */
	void (*samplefunc)(ImageReader *image, int x, int y, float offset, float color[4]);
	float offset1, offset2, weight1, weight2;

	if (fmaxf(right, left) > fmaxf(bottom, top)) { /* max(horizontal) > max(vertical) */
		samplefunc = sampleOffsetHorizontal;
		offset1 = right;
		offset2 = -left;
		weight1 = right / (right + left);
		weight2 = left / (right + left);
	}
	else {
		samplefunc = sampleOffsetVertical;
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
}

/*-----------------------------------------------------------------------------*/

}
/* smaa.cpp ends here */
