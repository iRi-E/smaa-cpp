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

/* smaa.h */

#ifndef SMAA_H
#define SMAA_H

#include "smaa_types.h"

namespace SMAA {

/*-----------------------------------------------------------------------------*/
/* SMAA Preset types */

enum CONFIG_PRESET {
	CONFIG_PRESET_LOW,
	CONFIG_PRESET_MEDIUM,
	CONFIG_PRESET_HIGH,
	CONFIG_PRESET_ULTRA,
	CONFIG_PRESET_EXTREME,
};

/*-----------------------------------------------------------------------------*/
/* SMAA Pixel Shaders */

class PixelShader {

private:
	bool m_enable_diag_detection;
	bool m_enable_corner_detection;
	bool m_enable_predication;
	float m_threshold;
	float m_depth_threshold;
	int m_max_search_steps;
	int m_max_search_steps_diag;
	int m_corner_rounding;
	float m_local_contrast_adaptation_factor;
	float m_predication_threshold;
	float m_predication_scale;
	float m_predication_strength;

public:
	PixelShader() { setPresets(CONFIG_PRESET_HIGH); }
	PixelShader(int preset) { setPresets(preset); }

	/*-----------------------------------------------------------------------------*/
	/* SMAA Presets */

	void setPresets(int preset)
	{
		m_enable_diag_detection = true;
		m_enable_corner_detection = true;
		m_enable_predication = false;
		m_threshold = 0.1;
		m_depth_threshold = 0.1;
		m_max_search_steps = 16;
		m_max_search_steps_diag = 8;
		m_corner_rounding = 25;
		m_local_contrast_adaptation_factor = 2.0;
		m_predication_threshold = 0.01;
		m_predication_scale = 2.0;
		m_predication_strength = 0.4;

		switch (preset) {
			case CONFIG_PRESET_LOW:
				m_threshold = 0.15;
				m_max_search_steps = 4;
				m_enable_diag_detection = false;
				m_enable_corner_detection = false;
				break;
			case CONFIG_PRESET_MEDIUM:
				m_threshold = 0.1;
				m_max_search_steps = 8;
				m_enable_diag_detection = false;
				m_enable_corner_detection = false;
				break;
			case CONFIG_PRESET_HIGH:
				m_threshold = 0.1;
				m_max_search_steps = 16;
				m_max_search_steps_diag = 8;
				m_corner_rounding = 25;
				break;
			case CONFIG_PRESET_ULTRA:
				m_threshold = 0.05;
				m_max_search_steps = 32;
				m_max_search_steps_diag = 16;
				m_corner_rounding = 25;
				break;
			case CONFIG_PRESET_EXTREME:
				m_threshold = 0.05;
				m_max_search_steps = 104;
				m_max_search_steps_diag = 18;
				m_corner_rounding = 25;
				break;
		}
	}

	/*-----------------------------------------------------------------------------*/
	/* Set/get parameters */

	/**
	 * Specify whether to enable diagonal processing.
	 */
	void setEnableDiagDetection(bool enable) { m_enable_diag_detection = enable; }
	bool getEnableDiagDetection() { return m_enable_diag_detection; }

	/**
	 * Specify whether to enable corner processing.
	 */
	void setEnableCornerDetection(bool enable) { m_enable_corner_detection = enable; }
	bool getEnableCornerDetection() { return m_enable_corner_detection; }

	/**
	 * Specify whether to enable predicated thresholding.
	 *
	 * Predicated thresholding allows to better preserve texture details and to
	 * improve performance, by decreasing the number of detected edges using an
	 * additional buffer like the light accumulation buffer, object ids or even the
	 * depth buffer (the depth buffer usage may be limited to indoor or short range
	 * scenes).
	 *
	 * It locally decreases the luma or color threshold if an edge is found in an
	 * additional buffer (so the global threshold can be higher).
	 *
	 * This method was developed by Playstation EDGE MLAA team, and used in
	 * Killzone 3, by using the light accumulation buffer. More information here:
	 *     http://iryoku.com/aacourse/downloads/06-MLAA-on-PS3.pptx
	 */
	void setEnablePredication(bool enable) { m_enable_predication = enable; }
	bool getEnablePredication() { return m_enable_predication; }

	/**
	 * Specify the threshold or sensitivity to edges.
	 * Lowering this value you will be able to detect more edges at the expense of
	 * performance.
	 *
	 * Range: [0, 0.5]
	 *   0.1 is a reasonable value, and allows to catch most visible edges.
	 *   0.05 is a rather overkill value, that allows to catch 'em all.
	 *
	 *   If temporal supersampling is used, 0.2 could be a reasonable value, as low
	 *   contrast edges are properly filtered by just 2x.
	 */
	void setThreshold(float threshold) { m_threshold = threshold; }
	float getThreshold() { return m_threshold; }

	/**
	 * Specify the threshold for depth edge detection.
	 *
	 * Range: depends on the depth range of the scene.
	 */

	void setDepthThreshold(float threshold) { m_depth_threshold = threshold; }
	float getDepthThreshold() { return m_depth_threshold; }

	/**
	 * Specify the maximum steps performed in the
	 * horizontal/vertical pattern searches, at each side of the pixel.
	 *
	 * In number of pixels, it's actually the double. So the maximum line length
	 * perfectly handled by, for example 16, is 64 (by perfectly, we meant that
	 * longer lines won't look as good, but still antialiased).
	 *
	 * Range: [0, 104]  (max < (14.5^2 - 2) / 2 = 104.25)
	 */
	void setMaxSearchSteps(int steps) { m_max_search_steps = steps; }
	int getMaxSearchSteps() { return m_max_search_steps; }
	/**
	 * Specify the maximum steps performed in the
	 * diagonal pattern searches, at each side of the pixel. In this case we jump
	 * one pixel at time, instead of two.
	 *
	 * Range: [0, 18]  (max < 18.5)
	 *
	 * On high-end machines it is cheap (between a 0.8x and 0.9x slower for 16
	 * steps), but it can have a significant impact on older machines.
	 *
	 * setEnableDiagDetection() to disable diagonal processing.
	 */
	void setMaxSearchStepsDiag(int steps) { m_max_search_steps_diag = steps; }
	int getMaxSearchStepsDiag() { return m_max_search_steps_diag; }

	/**
	 * Specify how much sharp corners will be rounded.
	 *
	 * Range: [0, 100]
	 *
	 * Use setEnableCornerDetection() to disable corner processing.
	 */
	void setCornerRounding(int rounding) { m_corner_rounding = rounding; }
	int getCornerRounding() { return m_corner_rounding; }

	/**
	 * Specify the local contrast adaptation factor.
	 *
	 * If there is an neighbor edge that has this factor times
	 * bigger contrast than current edge, current edge will be discarded.
	 *
	 * This allows to eliminate spurious crossing edges, and is based on the fact
	 * that, if there is too much contrast in a direction, that will hide
	 * perceptually contrast in the other neighbors.
	 */
	void setLocalContrastAdaptationFactor(float factor) { m_local_contrast_adaptation_factor = factor; }
	float getLocalContrastAdaptationFactor() { return m_local_contrast_adaptation_factor; }

	/**
	 * Specify threshold to be used in the additional predication buffer.
	 *
	 * Range: depends on the input, so you'll have to find the magic number that
	 * works for you.
	 */
	void setPredicationThreshold(float threshold) { m_predication_threshold = threshold; }
	float getPredicationThreshold() { return m_predication_threshold; }

	/**
	 * Specify how much to scale the global threshold used for luma or color edge
	 * detection when using predication.
	 *
	 * Range: [1, 5]
	 */
	void setPredicationScale(float scale) { m_predication_scale = scale; }
	float getPredicationScale() { return m_predication_scale; }

	/**
	 * Specify how much to locally decrease the threshold.
	 *
	 * Range: [0, 1]
	 */
	void setPredicationStrength(float strength) { m_predication_strength = strength; }
	float getPredicationStrength() { return m_predication_strength; }

	/*-----------------------------------------------------------------------------*/
	/* Edge Detection Pixel Shaders (First Pass) */

	/**
	 * Luma Edge Detection
	 *
	 * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and
	 * thus 'colorImage' should be a non-sRGB texture.
	 */
	void lumaEdgeDetection(int x, int y,
			       ImageReader *colorImage,
			       ImageReader *predicationImage,
			       /* out */ float edges[4]);

	/**
	 * Color Edge Detection
	 *
	 * IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and
	 * thus 'colorImage' should be a non-sRGB texture.
	 */
	void colorEdgeDetection(int x, int y,
				ImageReader *colorImage,
				ImageReader *predicationImage,
				/* out */ float edges[4]);

	/**
	 * Depth Edge Detection
	 */
	void depthEdgeDetection(int x, int y,
				ImageReader *depthImage,
				/* out */ float edges[4]);

	/*-----------------------------------------------------------------------------*/
	/* Blending Weight Calculation Pixel Shader (Second Pass) */

	/**
	 * Blending Weight Calculation Pixel Shader (Second Pass)
	 *   Just pass zero to subsampleIndices for SMAA 1x, see @SUBSAMPLE_INDICES.
	 */
	void blendingWeightCalculation(int x, int y,
				       ImageReader *edgesImage,
				       const float subsampleIndices[4],
				       /* out */ float weights[4]);

	/*-----------------------------------------------------------------------------*/
	/* Neighborhood Blending Pixel Shader (Third Pass) */

	/**
	 * Neighborhood Blending Pixel Shader (Third Pass)
	 */
	void neighborhoodBlending(int x, int y,
				  ImageReader *colorImage,
				  ImageReader *blendImage,
				  /* out */ float color[4]);

private:
	/* Internal */
	void calculatePredicatedThreshold(int x, int y, ImageReader *predicationImage, float threshold[2]);
	int searchDiag1(ImageReader *edgesImage, int x, int y, int dx, int dy, float *end, bool *found);
	int searchDiag2(ImageReader *edgesImage, int x, int y, int dx, int dy, float *end, bool *found);
	void calculateDiagWeights(ImageReader *edgesImage, int x, int y, const float edges[2],
				  const float subsampleIndices[4], float weights[2]);
	int searchXLeft(ImageReader *edgesImage, int x, int y);
	int searchXRight(ImageReader *edgesImage, int x, int y);
	int searchYUp(ImageReader *edgesImage, int x, int y);
	int searchYDown(ImageReader *edgesImage, int x, int y);
	void detectHorizontalCornerPattern(ImageReader *edgesImage, float weights[4],
					   int left, int right, int y, int d1, int d2);
	void detectVerticalCornerPattern(ImageReader *edgesImage, float weights[4],
					 int top, int bottom, int x, int d1, int d2);
};

}
#endif /* SMAA_H */
/* smaa.h ends here */
