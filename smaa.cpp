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

#define SMAA_AREATEX_SIZE 80 /* 16 * 5 = 20 * 4 = 80 */
#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_RGB2BW_WEIGHTS (Col4(0.2126, 0.7152, 0.0722, 0.0))

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

static float rgb2bw(Col4 color)
{
	return (SMAA_RGB2BW_WEIGHTS.r * color.r +
		SMAA_RGB2BW_WEIGHTS.g * color.g +
		SMAA_RGB2BW_WEIGHTS.b * color.b);
}

static Col4 sample(ImageReader *image, Vec2 texcoord)
{
	Vec2 i = texcoord.apply(floorf);
	Vec2 f = texcoord - i;
	Int2 ii = (Int2)i;

	Col4 color00 = image->getPixel(ii + Int2(0, 0));
	Col4 color10 = image->getPixel(ii + Int2(1, 0));
	Col4 color01 = image->getPixel(ii + Int2(0, 1));
	Col4 color11 = image->getPixel(ii + Int2(1, 1));

	return Col4(bilinear(color00.r, color10.r, color01.r, color11.r, f.x, f.y),
		    bilinear(color00.g, color10.g, color01.g, color11.g, f.x, f.y),
		    bilinear(color00.b, color10.b, color01.b, color11.b, f.x, f.y),
		    bilinear(color00.a, color10.a, color01.a, color11.a, f.x, f.y));
}

static Col4 sampleOffsetVertical(ImageReader *image, Int2 texcoord, float yoffset)
{
	float iy = floorf(yoffset);
	float fy = yoffset - iy;
	texcoord.y += (int)iy;

	Col4 color00 = image->getPixel(texcoord + Int2(0, 0));
	Col4 color01 = image->getPixel(texcoord + Int2(0, 1));

	return Col4(lerp(color00.r, color01.r, fy),
		    lerp(color00.g, color01.g, fy),
		    lerp(color00.b, color01.b, fy),
		    lerp(color00.a, color01.a, fy));
}

static Col4 sampleOffsetHorizontal(ImageReader *image, Int2 texcoord, float xoffset)
{
	float ix = floorf(xoffset);
	float fx = xoffset - ix;
	texcoord.x += (int)ix;

	Col4 color00 = image->getPixel(texcoord + Int2(0, 0));
	Col4 color10 = image->getPixel(texcoord + Int2(1, 0));

	return Col4(lerp(color00.r, color10.r, fx),
		    lerp(color00.g, color10.g, fx),
		    lerp(color00.b, color10.b, fx),
		    lerp(color00.a, color10.a, fx));
}

static const float* areatex_sample_internal(const float *areatex, int x, int y)
{
	return &areatex[(clamp(x, SMAA_AREATEX_SIZE) +
			 clamp(y, SMAA_AREATEX_SIZE) * SMAA_AREATEX_SIZE) * 2];
}

static Vec2 areaTexSampleLevelZero(const float *areatex, Vec2 texcoord)
{
	Vec2 i = texcoord.apply(floorf);
	Vec2 f = texcoord - i;
	int X = (int)i.x, Y = (int)i.y;

	const float *weights00 = areatex_sample_internal(areatex, X + 0, Y + 0);
	const float *weights10 = areatex_sample_internal(areatex, X + 1, Y + 0);
	const float *weights01 = areatex_sample_internal(areatex, X + 0, Y + 1);
	const float *weights11 = areatex_sample_internal(areatex, X + 1, Y + 1);

	return Vec2(bilinear(weights00[0], weights10[0], weights01[0], weights11[0], f.x, f.y),
		    bilinear(weights00[1], weights10[1], weights01[1], weights11[1], f.x, f.y));
}

/**
 * Gathers current pixel, and the top-left neighbors.
 */
static Vec3 gatherNeighbors(Int2 texcoord, ImageReader *image)
{
	Vec3 neighbors = {image->getPixel(texcoord).r,
			  image->getPixel(texcoord + Int2(-1, 0)).r, /* left */
			  image->getPixel(texcoord + Int2(0, -1)).r}; /* top */
	return neighbors;
}

/**
 * Adjusts the threshold by means of predication.
 */
Vec2 PixelShader::calculatePredicatedThreshold(Int2 texcoord, ImageReader *predicationImage)
{
	Vec3 neighbors = gatherNeighbors(texcoord, predicationImage);
	Vec2 delta = {fabsf(neighbors.x - neighbors.y), fabsf(neighbors.x - neighbors.z)};
	Vec2 edges = {step(m_predication_threshold, delta.x), step(m_predication_threshold, delta.y)};
	Vec2 threshold = Vec2(m_predication_scale * m_threshold) * (Vec2(1.0) - Vec2(m_predication_strength) * edges);
	return threshold;
}

/*-----------------------------------------------------------------------------*/
/* Edge Detection Pixel Shaders (First Pass) */

/**
 * Luma Edge Detection
 *
 * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and
 * thus 'colorImage' should be a non-sRGB image.
 */
Vec2 PixelShader::lumaEdgeDetection(Int2 texcoord, ImageReader *colorImage, ImageReader *predicationImage)
{
	Vec2 threshold = Vec2(m_threshold);

	/* Calculate the threshold: */
	if (m_enable_predication && predicationImage)
		Vec2 threshold = calculatePredicatedThreshold(texcoord, predicationImage);

	/* Calculate lumas: */
	float L = rgb2bw(colorImage->getPixel(texcoord));

	float Lleft = rgb2bw(colorImage->getPixel(texcoord + Int2(-1, 0)));
	float Ltop  = rgb2bw(colorImage->getPixel(texcoord + Int2(0, -1)));

	/* We do the usual threshold: */
	float delta_x = fabsf(L - Lleft);
	float delta_y = fabsf(L - Ltop);
	Vec2 edges = {step(threshold.x, delta_x), step(threshold.y, delta_y)};

	/* Then discard if there is no edge: */
	if (edges.x != 0.0 || edges.y != 0.0) {

		/* Calculate right and bottom deltas: */
		float Lright = rgb2bw(colorImage->getPixel(texcoord + Int2(1, 0)));
		float Lbottom = rgb2bw(colorImage->getPixel(texcoord + Int2(0, 1)));
		float delta_z = fabsf(L - Lright);
		float delta_w = fabsf(L - Lbottom);

		/* Calculate the maximum delta in the direct neighborhood: */
		float maxDelta_x = fmaxf(delta_x, delta_z);
		float maxDelta_y = fmaxf(delta_y, delta_w);

		/* Calculate left-left and top-top deltas: */
		float Lleftleft = rgb2bw(colorImage->getPixel(texcoord + Int2(-2, 0)));
		float Ltoptop = rgb2bw(colorImage->getPixel(texcoord + Int2(0, -2)));
		delta_z = fabsf(Lleft - Lleftleft);
		delta_w = fabsf(Ltop - Ltoptop);

		/* Calculate the final maximum delta: */
		maxDelta_x = fmaxf(maxDelta_x, delta_z);
		maxDelta_y = fmaxf(maxDelta_y, delta_w);
		{
			float finalDelta = fmaxf(maxDelta_x, maxDelta_y);

			/* Local contrast adaptation: */
			edges.x *= step(finalDelta, m_local_contrast_adaptation_factor * delta_x);
			edges.y *= step(finalDelta, m_local_contrast_adaptation_factor * delta_y);
		}
	}
	return edges;
}

/**
 * Color Edge Detection
 *
 * IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and
 * thus 'colorImage' should be a non-sRGB image.
 */
Vec2 PixelShader::colorEdgeDetection(Int2 texcoord, ImageReader *colorImage, ImageReader *predicationImage)
{
	Vec2 threshold = {m_threshold, m_threshold};

	/* Calculate the threshold: */
	if (m_enable_predication && predicationImage)
		Vec2 threshold = calculatePredicatedThreshold(texcoord, predicationImage);

	/* Calculate color deltas: */
	Vec4 delta;
	Col4 C = colorImage->getPixel(texcoord);

	Col4 Cleft = colorImage->getPixel(texcoord + Int2(-1, 0));
	Col4 Ctop  = colorImage->getPixel(texcoord + Int2(0, -1));
	float delta_x = fmaxf(fmaxf(fabsf(C.r - Cleft.r), fabsf(C.g - Cleft.g)), fabsf(C.b - Cleft.b));
	float delta_y = fmaxf(fmaxf(fabsf(C.r - Ctop.r), fabsf(C.g - Ctop.g)), fabsf(C.b - Ctop.b));

	/* We do the usual threshold: */
	Vec2 edges = {step(threshold.x, delta_x), step(threshold.y, delta_y)};

	/* Then discard if there is no edge: */
	if (edges.x != 0.0 || edges.y != 0.0) {

		/* Calculate right and bottom deltas: */
		Col4 Cright = colorImage->getPixel(texcoord + Int2(1, 0));
		Col4 Cbottom  = colorImage->getPixel(texcoord + Int2(0, 1));
		float delta_z = fmaxf(fmaxf(fabsf(C.r - Cright.r), fabsf(C.g - Cright.g)), fabsf(C.b - Cright.b));
		float delta_w = fmaxf(fmaxf(fabsf(C.r - Cbottom.r), fabsf(C.g - Cbottom.g)), fabsf(C.b - Cbottom.b));

		/* Calculate the maximum delta in the direct neighborhood: */
		float maxDelta_x = fmaxf(delta_x, delta_z);
		float maxDelta_y = fmaxf(delta_y, delta_w);

		/* Calculate left-left and top-top deltas: */
		Col4 Cleftleft = colorImage->getPixel(texcoord + Int2(-2, 0));
		Col4 Ctoptop = colorImage->getPixel(texcoord + Int2(0, -2));
		delta_z = fmaxf(fmaxf(fabsf(C.r - Cleftleft.r), fabsf(C.g - Cleftleft.g)), fabsf(C.b - Cleftleft.b));
		delta_w = fmaxf(fmaxf(fabsf(C.r - Ctoptop.r), fabsf(C.g - Ctoptop.g)), fabsf(C.b - Ctoptop.b));

		/* Calculate the final maximum delta: */
		maxDelta_x = fmaxf(maxDelta_x, delta_z);
		maxDelta_y = fmaxf(maxDelta_y, delta_w);
		{
			float finalDelta = fmaxf(maxDelta_x, maxDelta_y);

			/* Local contrast adaptation: */
			edges.x *= step(finalDelta, m_local_contrast_adaptation_factor * delta_x);
			edges.y *= step(finalDelta, m_local_contrast_adaptation_factor * delta_y);
		}
	}
	return edges;
}

/**
 * Depth Edge Detection
 */
Vec2 PixelShader::depthEdgeDetection(Int2 texcoord, ImageReader *depthImage)
{
	Vec3 neighbors = gatherNeighbors(texcoord, depthImage);
	Vec2 delta = {fabsf(neighbors.x - neighbors.y), fabsf(neighbors.x - neighbors.z)};
	Vec2 edges = {step(m_depth_threshold, delta.x), step(m_depth_threshold, delta.y)};

	return edges;
}

/*-----------------------------------------------------------------------------*/
/* Diagonal Search Functions */

/**
 * These functions allows to perform diagonal pattern searches.
 */
Int2 PixelShader::searchDiag1(ImageReader *edgesImage, Int2 texcoord, Int2 dir, /* out */ Vec2 *e)
{
	Int2 coord = {-1, (int)true};
	while (coord.x < (m_max_search_steps_diag - 1) &&
	       coord.y) {
		texcoord += dir;
		coord.x += 1;
		*e = edgesImage->getPixel(texcoord).rg();
		coord.y = (int)(e->x > 0.9 && e->y > 0.9); /* note: e = 0.0 or 1.0 */
	}
	return coord;
}

Int2 PixelShader::searchDiag2(ImageReader *edgesImage, Int2 texcoord, Int2 dir, /* out */ Vec2 *e)
{
	Int2 coord = {-1, (int)true};
	while (coord.x < (m_max_search_steps_diag - 1) &&
	       coord.y) {
		texcoord += dir;
		coord.x += 1;
		*e = Vec2(edgesImage->getPixel(texcoord + Int2(1, 0)).r,
			  edgesImage->getPixel(texcoord).g);
		coord.y = (int)(e->x > 0.9 && e->y > 0.9);
	}
	return coord;
}

/**
 * Similar to area(), this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
static Vec2 areaDiag(Int2 dist, Int2 e, float offset)
{
	Vec2 texcoord = Vec2(Int2(SMAA_AREATEX_MAX_DISTANCE_DIAG) * e + dist);

	/* We do a bias for mapping to texel space: */
	texcoord += Vec2(0.5);

	/* Move to proper place, according to the subpixel offset: */
	texcoord.y += (float)SMAA_AREATEX_SIZE * offset;

	/* Do it! */
	return areaTexSampleLevelZero(areatex_diag, texcoord);
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
Vec2 PixelShader::calculateDiagWeights(ImageReader *edgesImage, Int2 texcoord, Vec2 e, Vec4 subsampleIndices)
{
	Vec2 weights = Vec2(0.0);

	/* Search for the line ends: */
	Int2 d1, d2;
	Vec2 end;
	if (e.x > 0.0) {
		d1 = searchDiag1(edgesImage, texcoord, Int2(-1, 1), &end);
		d1.x += (int)end.y;
	} else
		d1 = Int2(0, (int)false);
	d2 = searchDiag1(edgesImage, texcoord, Int2(1, -1), &end);

	if (d1.x + d2.x > 2) { /* d.x + d.y + 1 > 3 */
		Int2 cc = Int2(0);
		if (!(bool)d1.y) {
			/* Fetch the crossing edges: */
			Int2 coords = texcoord + Int2(-d1.x, d1.x);
			Int2 c = (Int2)Vec2(edgesImage->getPixel(coords + Int2(-1,  0)).g,
					    edgesImage->getPixel(coords + Int2( 0,  0)).r);

			/* Merge crossing edges at each side into a single value: */
			cc.x = 2 * c.x + c.y;
		}
		if (!(bool)d2.y) {
			/* Fetch the crossing edges: */
			Int2 coords = texcoord + Int2(d2.x, -d2.x);
			Int2 c = (Int2)Vec2(edgesImage->getPixel(coords + Int2( 1,  0)).g,
					    edgesImage->getPixel(coords + Int2( 1, -1)).r);

			/* Merge crossing edges at each side into a single value: */
			cc.y = 2 * c.x + c.y;
		}

		/* Fetch the areas for this line: */
		weights += areaDiag(Int2(d1.x, d2.x), cc, subsampleIndices.z);
	}

	/* Search for the line ends: */
	d1 = searchDiag2(edgesImage, texcoord, Int2(-1, -1), &end);
	if (edgesImage->getPixel(texcoord + Int2(1, 0)).r > 0.0) {
		d2 = searchDiag2(edgesImage, texcoord, Int2(1, 1), &end);
		d2.x += (int)end.y;
	} else
		d2 = Int2(0, (int)false);

	if (d1.x + d2.x > 2) { /* d.x + d.y + 1 > 3 */
		Int2 cc = Int2(0);
		if (!(bool)d1.y) {
			/* Fetch the crossing edges: */
			Int2 coords = texcoord + Int2(-d1.x, -d1.x);
			Int2 c = (Int2)Vec2(edgesImage->getPixel(coords + Int2(-1,  0)).g,
					    edgesImage->getPixel(coords + Int2( 0, -1)).r);
			cc.x = 2 * c.x + c.y;
		}
		if (!(bool)d2.y) {
			Int2 coords = texcoord + Int2(d2.x, d2.x);
			Int2 c = edgesImage->getPixel(coords + Int2( 1,  0)).gr();
			cc.y = 2 * c.x + c.y;
		}

		/* Fetch the areas for this line: */
		weights += areaDiag(Int2(d1.x, d2.x), cc, subsampleIndices.w).yx();
	}

	return weights;
}

/*-----------------------------------------------------------------------------*/
/* Horizontal/Vertical Search Functions */

int PixelShader::searchXLeft(ImageReader *edgesImage, Int2 texcoord)
{
	int end = texcoord.x - 2 * m_max_search_steps;
	Col4 e;
	while (texcoord.x >= end) {
		e = edgesImage->getPixel(texcoord);
		if (e.g == 0.0 || /* Is the edge not activated? */
		    e.r != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(0, -1)).r != 0.0)
			break;
		texcoord.x += -1;
		e = edgesImage->getPixel(texcoord);
		if (e.g == 0.0 || /* Is the edge not activated? */
		    e.r != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(0, -1)).r != 0.0)
			break;
		texcoord.x += -1;
	}
	return texcoord.x;
}

int PixelShader::searchXRight(ImageReader *edgesImage, Int2 texcoord)
{
	int end = texcoord.x + 2 * m_max_search_steps + 1;
	Col4 e;
	while (texcoord.x <= end) {
		texcoord.x += 1;
		e = edgesImage->getPixel(texcoord);
		if (e.g == 0.0 || /* Is the edge not activated? */
		    e.r != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(0, -1)).r != 0.0)
			break;
		texcoord.x += 1;
		e = edgesImage->getPixel(texcoord);
		if (e.g == 0.0 || /* Is the edge not activated? */
		    e.r != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(0, -1)).r != 0.0)
			break;
	}
	return texcoord.x - 1;
}

int PixelShader::searchYUp(ImageReader *edgesImage, Int2 texcoord)
{
	int end = texcoord.y - 2 * m_max_search_steps;
	Col4 e;
	while (texcoord.y >= end) {
		e = edgesImage->getPixel(texcoord);
		if (e.r == 0.0 || /* Is the edge not activated? */
		    e.g != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(-1, 0)).g != 0.0)
			break;
		texcoord.y += -1;
		e = edgesImage->getPixel(texcoord);
		if (e.r == 0.0 || /* Is the edge not activated? */
		    e.g != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(-1, 0)).g != 0.0)
			break;
		texcoord.y += -1;
	}
	return texcoord.y;
}

int PixelShader::searchYDown(ImageReader *edgesImage, Int2 texcoord)
{
	int end = texcoord.y + 2 * m_max_search_steps + 1;
	Col4 e;
	while (texcoord.y <= end) {
		texcoord.y += 1;
		e = edgesImage->getPixel(texcoord);
		if (e.r == 0.0 || /* Is the edge not activated? */
		    e.g != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(-1, 0)).g != 0.0)
			break;
		texcoord.y += 1;
		e = edgesImage->getPixel(texcoord);
		if (e.r == 0.0 || /* Is the edge not activated? */
		    e.g != 0.0 || /* Or is there a crossing edge that breaks the line? */
		    edgesImage->getPixel(texcoord + Int2(-1, 0)).g != 0.0)
			break;
	}
	return texcoord.y - 1;
}

/**
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
static Vec2 area(Vec2 dist, float e1, float e2, float offset)
{
	/* Rounding prevents precision errors of bilinear filtering: */
	Vec2 texcoord = Vec2(SMAA_AREATEX_MAX_DISTANCE) * (Vec2(4.0) * Vec2(e1, e2)).apply(roundf) + dist;

	/* We do a bias for mapping to texel space: */
	texcoord += Vec2(0.5);

	/* Move to proper place, according to the subpixel offset: */
	texcoord.y += (float)SMAA_AREATEX_SIZE * offset;

	/* Do it! */
	return areaTexSampleLevelZero(areatex, texcoord);
}

/*-----------------------------------------------------------------------------*/
/*  Corner Detection Functions */

void PixelShader::detectHorizontalCornerPattern(ImageReader *edgesImage, /* inout */ Col4 *weights,
						int left, int right, int y, Int2 d)
{
	Vec2 factor = Vec2(1.0);
	float rounding = 1.0 - (float)m_corner_rounding / 100.0;

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d.x == d.y) ? 0.5 : 1.0;

	/* Near the left corner */
	if (d.x <= d.y) {
		factor -= Vec2(rounding) * Vec2(edgesImage->getPixel(Int2(left, y) + Int2(0,  1)).r,
						edgesImage->getPixel(Int2(left, y) + Int2(0, -2)).r);
	}
	/* Near the right corner */
	if (d.x >= d.y) {
		factor -= Vec2(rounding) * Vec2(edgesImage->getPixel(Int2(right, y) + Int2(1,  1)).r,
						edgesImage->getPixel(Int2(right, y) + Int2(1, -2)).r);
	}

	weights->r *= saturate(factor.x);
	weights->g *= saturate(factor.y);
}

void PixelShader::detectVerticalCornerPattern(ImageReader *edgesImage, /* inout */ Col4 *weights,
					      int top, int bottom, int x, Int2 d)
{
	Vec2 factor = Vec2(1.0);
	float rounding = 1.0 - (float)m_corner_rounding / 100.0;

	/* Reduce blending for pixels in the center of a line. */
	rounding *= (d.x == d.y) ? 0.5 : 1.0;

	/* Near the top corner */
	if (d.x <= d.y) {
		factor -= Vec2(rounding) * Vec2(edgesImage->getPixel(Int2(x, top) + Int2( 1, 0)).g,
						edgesImage->getPixel(Int2(x, top) + Int2(-2, 0)).g);
	}
	/* Near the bottom corner */
	if (d.x >= d.y) {
		factor -= Vec2(rounding) * Vec2(edgesImage->getPixel(Int2(x, bottom) + Int2( 1, 1)).g,
						edgesImage->getPixel(Int2(x, bottom) + Int2(-2, 1)).g);
	}

	weights->b *= saturate(factor.x);
	weights->a *= saturate(factor.y);
}

/*-----------------------------------------------------------------------------*/
/* Blending Weight Calculation Pixel Shader (Second Pass) */
/*   Just pass zero to subsampleIndices for SMAA 1x, see @SUBSAMPLE_INDICES. */

Col4 PixelShader::blendingWeightCalculation(Int2 texcoord, ImageReader *edgesImage, Vec4 subsampleIndices)
{
	Col4 weights = Col4(0.0);
	Vec2 w;

	Vec2 e = edgesImage->getPixel(texcoord).rg();

	if (e.y > 0.0) { /* Edge at north */
		if (m_enable_diag_detection) {
			/* Diagonals have both north and west edges, so searching for them in */
			/* one of the boundaries is enough. */
			w = calculateDiagWeights(edgesImage, texcoord, e, subsampleIndices);
			weights.r = w.x;
			weights.g = w.y;

			/* We give priority to diagonals, so if we find a diagonal we skip  */
			/* horizontal/vertical processing. */
			if (weights.r + weights.g != 0.0)
				return weights;
		}

		/* Find the distance to the left and the right: */
		int left = searchXLeft(edgesImage, texcoord);
		int right = searchXRight(edgesImage, texcoord);
		Int2 d = Int2(left - texcoord.x, right - texcoord.x).apply(abs);

		/* Now fetch the left and right crossing edges, two at a time using bilinear */
		/* filtering. Sampling at -0.25 enables to discern what value each edge has: */
		float e1 = sampleOffsetVertical(edgesImage, Int2(left, texcoord.y), -0.25).r;
		float e2 = sampleOffsetVertical(edgesImage, Int2(right + 1, texcoord.y), -0.25).r;

		/* area() below needs a sqrt, as the areas texture is compressed */
		/* quadratically: */
		Vec2 sqrt_d = Vec2(d).apply(sqrtf);

		/* Ok, we know how this pattern looks like, now it is time for getting */
		/* the actual area: */
		w = area(sqrt_d, e1, e2, subsampleIndices.y);
		weights.r = w.x;
		weights.g = w.y;

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectHorizontalCornerPattern(edgesImage, &weights, left, right, texcoord.y, d);
	}

	if (e.x > 0.0) { /* Edge at west */

		/* Find the distance to the top and the bottom: */
		int top = searchYUp(edgesImage, texcoord);
		int bottom = searchYDown(edgesImage, texcoord);
		Int2 d = Int2(top - texcoord.y, bottom - texcoord.y).apply(abs);

		/* Fetch the top ang bottom crossing edges: */
		float e1 = sampleOffsetHorizontal(edgesImage, Int2(texcoord.x, top), -0.25).g;
		float e2 = sampleOffsetHorizontal(edgesImage, Int2(texcoord.x, bottom + 1), -0.25).g;

		/* area() below needs a sqrt, as the areas texture is compressed  */
		/* quadratically: */
		Vec2 sqrt_d = Vec2(d).apply(sqrtf);

		/* Get the area for this direction: */
		w = area(sqrt_d, e1, e2, subsampleIndices.x);
		weights.b = w.x;
		weights.a = w.y;

		/* Fix corners: */
		if (m_enable_corner_detection)
			detectVerticalCornerPattern(edgesImage, &weights, top, bottom, texcoord.x, d);
	}

	return weights;
}

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending Pixel Shader (Third Pass) */

Col4 PixelShader::neighborhoodBlending(Int2 texcoord, ImageReader *colorImage, ImageReader *blendImage)
{
	/* Fetch the blending weights for current pixel: */
	Col4 c = blendImage->getPixel(texcoord);
	float left = c.b, top = c.r;
	float right = blendImage->getPixel(texcoord + Int2(1, 0)).a;
	float bottom = blendImage->getPixel(texcoord + Int2(0, 1)).g;

	/* Is there any blending weight with a value greater than 0.0? */
	if (right + bottom + left + top < 1e-5)
		return colorImage->getPixel(texcoord);

	/* Calculate the blending offsets: */
	Col4 (*samplefunc)(ImageReader *image, Int2 texcoord, float offset);
	float offset1, offset2, weight1, weight2;

	if (fmaxf(right, left) > fmaxf(bottom, top)) { /* max(horizontal) > max(vertical) */
		samplefunc = sampleOffsetHorizontal;
		offset1 = right;
		offset2 = -left;
		weight1 = right / (right + left);
		weight2 = left / (right + left);
	} else {
		samplefunc = sampleOffsetVertical;
		offset1 = bottom;
		offset2 = -top;
		weight1 = bottom / (bottom + top);
		weight2 = top / (bottom + top);
	}

	/* We exploit bilinear filtering to mix current pixel with the chosen neighbor: */
	Col4 color1 = samplefunc(colorImage, texcoord, offset1);
	Col4 color2 = samplefunc(colorImage, texcoord, offset2);

	return (Col4(weight1) * color1 + Col4(weight2) * color2);
}

/*-----------------------------------------------------------------------------*/

}
/* smaa.cpp ends here */
