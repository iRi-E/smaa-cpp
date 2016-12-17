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

/*
 * smaa_areatex.cpp  version 0.1.1
 *
 * This program is C++ rewrite of AreaTex.py included in SMAA ditribution.
 *
 *   SMAA in GitHub: https://github.com/iryoku/smaa
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cmath>

/*------------------------------------------------------------------------------*/
/* Type Definitions */

class Int2;
class Dbl2;

class Int2 {
public:
	int x, y;

	Int2() { this->x = this->y = 0; }
	Int2(int x) { this->x = this->y = x; }
	Int2(int x, int y) { this->x = x; this->y = y; }

	operator Dbl2();

	Int2 operator + (Int2 other) { return Int2(x + other.x, y + other.y); }
	Int2 operator * (Int2 other) { return Int2(x * other.x, y * other.y); }
};

class Dbl2 {
public:
	double x, y;

	Dbl2() { this->x = this->y = 0.0; }
	Dbl2(double x) { this->x = this->y = x; }
	Dbl2(double x, double y) { this->x = x; this->y = y; }

	Dbl2 apply(double (* func)(double)) { return Dbl2(func(x), func(y)); }

	operator Int2();

	Dbl2 operator + (Dbl2 other) { return Dbl2(x + other.x, y + other.y); }
	Dbl2 operator - (Dbl2 other) { return Dbl2(x - other.x, y - other.y); }
	Dbl2 operator * (Dbl2 other) { return Dbl2(x * other.x, y * other.y); }
	Dbl2 operator / (Dbl2 other) { return Dbl2(x / other.x, y / other.y); }
	Dbl2 operator += (Dbl2 other) { return Dbl2(x += other.x, y += other.y); }
};

Int2::operator Dbl2() { return Dbl2((double)x, (double)y); }
Dbl2::operator Int2() { return Int2((int)x, (int)y); }

/*------------------------------------------------------------------------------*/
/* Data to Calculate Areatex */

static const double subsample_offsets_ortho[] = {0.0,    /* 0 */
						 -0.25,  /* 1 */
						 0.25,   /* 2 */
						 -0.125, /* 3 */
						 0.125,  /* 4 */
						 -0.375, /* 5 */
						 0.375}; /* 6 */

static const Dbl2 subsample_offsets_diag[] = {{ 0.00,   0.00},   /* 0 */
					      { 0.25,  -0.25},   /* 1 */
					      {-0.25,   0.25},   /* 2 */
					      { 0.125, -0.125},  /* 3 */
					      {-0.125,  0.125}}; /* 4 */

/* Texture sizes: */
/* (it's quite possible that this is not easily configurable) */
#define SIZE_ORTHO 16 /* 16 * 5 slots = 80 */
#define SIZE_DIAG  20 /* 20 * 4 slots = 80 */

/* Number of samples for calculating areas in the diagonal textures: */
/* (diagonal areas are calculated using brute force sampling) */
#define SAMPLES_DIAG 30

/* Maximum distance for smoothing u-shapes: */
#define SMOOTH_MAX_DISTANCE 32

/*------------------------------------------------------------------------------*/
/* Miscellaneous Utility Functions */

/* Linear interpolation: */
static Dbl2 lerp(Dbl2 a, Dbl2 b, double p)
{
	return a + (b - a) * Dbl2(p);
}

/* Saturates a value to [0..1] range: */
static double saturate(double x)
{
	return 0.0 < x ? (x < 1.0 ? x : 1.0) : 0.0;
}

/*------------------------------------------------------------------------------*/
/* Mapping Tables (for placing each pattern subtexture into its place) */

enum edgesorthoIndices
{
	EDGESORTHO_NONE_NONE = 0,
	EDGESORTHO_NONE_NEGA = 1,
	EDGESORTHO_NONE_POSI = 2,
	EDGESORTHO_NONE_BOTH = 3,
	EDGESORTHO_NEGA_NONE = 4,
	EDGESORTHO_NEGA_NEGA = 5,
	EDGESORTHO_NEGA_POSI = 6,
	EDGESORTHO_NEGA_BOTH = 7,
	EDGESORTHO_POSI_NONE = 8,
	EDGESORTHO_POSI_NEGA = 9,
	EDGESORTHO_POSI_POSI = 10,
	EDGESORTHO_POSI_BOTH = 11,
	EDGESORTHO_BOTH_NONE = 12,
	EDGESORTHO_BOTH_NEGA = 13,
	EDGESORTHO_BOTH_POSI = 14,
	EDGESORTHO_BOTH_BOTH = 15,
};

static const Int2 edgesortho[] = {{0, 0}, {0, 1}, {0, 3}, {0, 4}, {1, 0}, {1, 1}, {1, 3}, {1, 4},
				  {3, 0}, {3, 1}, {3, 3}, {3, 4}, {4, 0}, {4, 1}, {4, 3}, {4, 4}};

enum edgesdiagIndices
{
	EDGESDIAG_NONE_NONE = 0,
	EDGESDIAG_NONE_VERT = 1,
	EDGESDIAG_NONE_HORZ = 2,
	EDGESDIAG_NONE_BOTH = 3,
	EDGESDIAG_VERT_NONE = 4,
	EDGESDIAG_VERT_VERT = 5,
	EDGESDIAG_VERT_HORZ = 6,
	EDGESDIAG_VERT_BOTH = 7,
	EDGESDIAG_HORZ_NONE = 8,
	EDGESDIAG_HORZ_VERT = 9,
	EDGESDIAG_HORZ_HORZ = 10,
	EDGESDIAG_HORZ_BOTH = 11,
	EDGESDIAG_BOTH_NONE = 12,
	EDGESDIAG_BOTH_VERT = 13,
	EDGESDIAG_BOTH_HORZ = 14,
	EDGESDIAG_BOTH_BOTH = 15,
};

static const Int2 edgesdiag[]  = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1}, {1, 2}, {1, 3},
				  {2, 0}, {2, 1}, {2, 2}, {2, 3}, {3, 0}, {3, 1}, {3, 2}, {3, 3}};

/*------------------------------------------------------------------------------*/
/* Horizontal/Vertical Areas */

/* Smoothing function for small u-patterns: */
static Dbl2 smootharea(double d, Dbl2 a1, Dbl2 a2)
{
	Dbl2 b1 = (a1 * Dbl2(2.0)).apply(sqrt) * Dbl2(0.5);
	Dbl2 b2 = (a2 * Dbl2(2.0)).apply(sqrt) * Dbl2(0.5);;
	double p = saturate(d / (double)SMOOTH_MAX_DISTANCE);
	return lerp(b1, a1, p) + lerp(b2, a2, p);
}

/* Calculates the area under the line p1->p2, for the pixel x..x+1: */
static Dbl2 area(Dbl2 p1, Dbl2 p2, int x)
{
	Dbl2 d = {p2.x - p1.x, p2.y - p1.y};
	double x1 = (double)x;
	double x2 = x + 1.0;
	double y1 = p1.y + d.y * (x1 - p1.x) / d.x;
	double y2 = p1.y + d.y * (x2 - p1.x) / d.x;

	if ((x1 >= p1.x && x1 < p2.x) || (x2 > p1.x && x2 <= p2.x)) { /* inside? */
		if ((copysign(1.0, y1) == copysign(1.0, y2) ||
		     fabs(y1) < 1e-4 || fabs(y2) < 1e-4)) { /* trapezoid? */
			double a = (y1 + y2) / 2.0;
			if (a < 0.0)
				return Dbl2(fabs(a), 0.0);
			else
				return Dbl2(0.0, fabs(a));
		}
		else { /* Then, we got two triangles: */
			double x = -p1.y * d.x / d.y + p1.x, xi;
			double a1 = x > p1.x ? y1 * modf(x, &xi) / 2.0 : 0.0;
			double a2 = x < p2.x ? y2 * (1.0 - modf(x, &xi)) / 2.0 : 0.0;
			double a = fabs(a1) > fabs(a2) ? a1 : -a2;
			if (a < 0.0)
				return Dbl2(fabs(a1), fabs(a2));
			else
				return Dbl2(fabs(a2), fabs(a1));
		}
	}
	else
		return Dbl2(0.0, 0.0);
}

/* Calculates the area for a given pattern and distances to the left and to the */
/* right, biased by an offset: */
static Dbl2 areaortho(int pattern, int left, int right, double offset)
{
	Dbl2 a1, a2;

	/*
	 * o1           |
	 *      .-------´
	 * o2   |
	 *
	 *      <---d--->
	 */
	double d = (double)(left + right + 1);

	double o1 = 0.5 + offset;
	double o2 = 0.5 + offset - 1.0;

	switch (pattern) {
		case EDGESORTHO_NONE_NONE:
		{
			/*
			 *
			 *    ------
			 *
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_NONE:
		{
			/*
			 *
			 *   .------
			 *   |
			 *
			 * We only offset L patterns in the crossing edge side, to make it
			 * converge with the unfiltered pattern 0 (we don't want to filter the
			 * pattern 0 to avoid artifacts).
			 */
			if (left <= right)
				return area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_NONE_POSI:
		{
			/*
			 *
			 *    ------.
			 *          |
			 */
			if (left >= right)
				return area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_POSI:
		{
			/*
			 *
			 *   .------.
			 *   |      |
			 */
			a1 = area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
			a2 = area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
			return smootharea(d, a1, a2);
			break;
		}
		case EDGESORTHO_NEGA_NONE:
		{
			/*
			 *   |
			 *   `------
			 *
			 */
			if (left <= right)
				return area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_BOTH_NONE:
		{
			/*
			 *   |
			 *   +------
			 *   |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_NEGA_POSI:
		{
			/*
			 *   |
			 *   `------.
			 *          |
			 *
			 * A problem of not offseting L patterns (see above), is that for certain
			 * max search distances, the pixels in the center of a Z pattern will
			 * detect the full Z pattern, while the pixels in the sides will detect a
			 * L pattern. To avoid discontinuities, we blend the full offsetted Z
			 * revectorization with partially offsetted L patterns.
			 */
			if (fabs(offset) > 0.0) {
				a1 = area(Dbl2(0.0, o1), Dbl2(d, o2), left);
				a2 = area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
				a2 += area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
				return (a1 + a2) / Dbl2(2.0);
			}
			else
				return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_BOTH_POSI:
		{
			/*
			 *   |
			 *   +------.
			 *   |      |
			 */
			return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_NONE_NEGA:
		{
			/*
			 *          |
			 *    ------´
			 *
			 */
			if (left >= right)
				return area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_NEGA:
		{
			/*
			 *          |
			 *   .------´
			 *   |
			 */
			if (fabs(offset) > 0.0) {
				a1 = area(Dbl2(0.0, o2), Dbl2(d, o1), left);
				a2 = area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
				a2 += area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
				return (a1 + a2) / Dbl2(2.0);
			}
			else
				return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NONE_BOTH:
		{
			/*
			 *          |
			 *    ------+
			 *          |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_BOTH:
		{
			/*
			 *          |
			 *   .------+
			 *   |      |
			 */
			return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NEGA_NEGA:
		{
			/*
			 *   |      |
			 *   `------´
			 *
			 */
			a1 = area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
			a2 = area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
			return smootharea(d, a1, a2);
			break;
		}
		case EDGESORTHO_BOTH_NEGA:
		{
			/*
			 *   |      |
			 *   +------´
			 *   |
			 */
			return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NEGA_BOTH:
		{
			/*
			 *   |      |
			 *   `------+
			 *          |
			 */
			return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_BOTH_BOTH:
		{
			/*
			 *   |      |
			 *   +------+
			 *   |      |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
	}

	return Dbl2(0.0, 0.0);
}

/*------------------------------------------------------------------------------*/
/* Diagonal Areas */

static bool inside(Dbl2 p1, Dbl2 p2, Dbl2 p)
{
	if (p1.x != p2.x || p1.y != p2.y) {
		double x = p.x, y = p.y;
		double xm = (p1.x + p2.x) / 2.0, ym = (p1.y + p2.y) / 2.0;
		double a = p2.y - p1.y;
		double b = p1.x - p2.x;
		return (a * (x - xm) + b * (y - ym) > 0);
	}
	else
                return true;
}

/* Calculates the area under the line p1->p2 for the pixel 'p' using brute */
/* force sampling: */
/* (quick and dirty solution, but it works) */
static double area1(Dbl2 p1, Dbl2 p2, Int2 p)
{
	int a = 0;

	for (int x = 0; x < SAMPLES_DIAG; x++) {
		for (int y = 0; y < SAMPLES_DIAG; y++) {
			if (inside(p1, p2, Dbl2((double)p.x + (double)x / (double)(SAMPLES_DIAG - 1),
						(double)p.y + (double)y / (double)(SAMPLES_DIAG - 1))))
				a++;
		}
	}
	return (double)a / (double)(SAMPLES_DIAG * SAMPLES_DIAG);
}

/* Calculates the area under the line p1->p2: */
/* (includes the pixel and its opposite) */
static Dbl2 area(int pattern, Dbl2 p1, Dbl2 p2, int left, Dbl2 offset)
{
	Int2 e = edgesdiag[pattern];
	if (e.x > 0)
		p1 += offset;
	if (e.y > 0)
		p2 += offset;
	double a1 = area1(p1, p2, Int2(1, 0) + Int2(left));
	double a2 = area1(p1, p2, Int2(1, 1) + Int2(left));
	return Dbl2(1.0 - a1, a2);
}

/* Calculates the area for a given pattern and distances to the left and to the */
/* right, biased by an offset: */
static Dbl2 areadiag(int pattern, int left, int right, Dbl2 offset)
{
	Dbl2 a1, a2;

	double d = (double)(left + right + 1);

	/*
	 * There is some Black Magic around diagonal area calculations. Unlike
	 * orthogonal patterns, the 'null' pattern (one without crossing edges) must be
	 * filtered, and the ends of both the 'null' and L patterns are not known: L
	 * and U patterns have different endings, and we don't know what is the
	 * adjacent pattern. So, what we do is calculate a blend of both possibilites.
	 */
	switch (pattern) {
		case EDGESDIAG_NONE_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset); /* 1st possibility */
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset); /* 2nd possibility */
			return (a1 + a2) / Dbl2(2.0); /* Blend them */
			break;
		}
		case EDGESDIAG_VERT_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 0.0), Dbl2(0.0, 0.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_NONE_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(pattern, Dbl2(0.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			return area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			break;
		}
		case EDGESDIAG_HORZ_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(0.0, 0.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_BOTH_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(0.0, 0.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			return area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			break;
		}
		case EDGESDIAG_BOTH_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_NONE_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(pattern, Dbl2(0.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			return area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			break;
		}
		case EDGESDIAG_NONE_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(pattern, Dbl2(0.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			return area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			break;
		}
		case EDGESDIAG_BOTH_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_BOTH_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(pattern, Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left, offset);
			a2 = area(pattern, Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left, offset);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
	}

	return Dbl2(0.0, 0.0);
}

/*------------------------------------------------------------------------------*/
/* Main Loops */

/* Buffers to Store AreaTex Data Temporarily */
static double ortho[7][5 * SIZE_ORTHO][5 * SIZE_ORTHO][2];
static double diag[5][4 * SIZE_DIAG][4 * SIZE_DIAG][2];

static void areatex_ortho(int offset_index)
{
	double offset = subsample_offsets_ortho[offset_index];

	for (int pattern = 0; pattern < 16; pattern++) {
		Int2 e = Int2(SIZE_ORTHO) * edgesortho[pattern];
		for (int left = 0; left < SIZE_ORTHO; left++) {
			for (int right = 0; right < SIZE_ORTHO; right++) {
				Dbl2 p = areaortho(pattern, left * left, right * right, offset);
				Int2 coords = e + Int2(left, right);

				ortho[offset_index][coords.y][coords.x][0] = p.x;
				ortho[offset_index][coords.y][coords.x][1] = p.y;
			}
		}
	}
	return;
}

static void areatex_diag(int offset_index)
{
	Dbl2 offset = subsample_offsets_diag[offset_index];

	for (int pattern = 0; pattern < 16; pattern++) {
		Int2 e = Int2(SIZE_DIAG) * edgesdiag[pattern];
		for (int left = 0; left < SIZE_DIAG; left++) {
			for (int right = 0; right < SIZE_DIAG; right++) {
				Dbl2 p = areadiag(pattern, left, right, offset);
				Int2 coords = e + Int2(left, right);

				diag[offset_index][coords.y][coords.x][0] = p.x;
				diag[offset_index][coords.y][coords.x][1] = p.y;
			}
		}
	}
	return;
}

/*------------------------------------------------------------------------------*/
/* Write File to Specified Location on Disk */

/* C/C++ source code (arrays of floats) */
static void write_double_array(FILE *fp, const double *ptr, int length, const char *array_name, bool quantize)
{
	fprintf(fp, "static const float %s[%d] = {", array_name, length);

	for (int n = 0; n < length; n++) {
		if (n > 0)
			fprintf(fp, ",");
		fprintf(fp, (n % 8 != 0) ? " " : "\n\t");

		if (quantize)
			fprintf(fp, "%3d / 255.0", (int)(*(ptr++) * 255.0));
		else
			fprintf(fp, "%1.8lf", *(ptr++));
	}

	fprintf(fp, "\n};\n");
}

static void write_csource(FILE *fp, bool subsampling, bool quantize)
{
	fprintf(fp, "/* This file was generated by smaa_areatex.cpp */\n");

	fprintf(fp, "\n/* Horizontal/Vertical Areas */\n");
	write_double_array(fp, (double *)ortho,
			   (5 * SIZE_ORTHO) * (5 * SIZE_ORTHO) * 2 * (subsampling ? 7 : 1),
			   "areatex", quantize);

	fprintf(fp, "\n/* Diagonal Areas */\n");
	write_double_array(fp, (double *)diag,
			   (4 * SIZE_DIAG) * (4 * SIZE_DIAG) * 2 * (subsampling ? 5 : 1),
			   "areatex_diag", quantize);
}

/* .tga File (RGBA 32bit uncompressed) */
static void write_tga(FILE *fp, bool subsampling)
{
	int samples = subsampling ? 7 : 1;
	unsigned char header[18] = {0, 0,
				    2,   /* uncompressed RGB */
				    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				    32,  /* 32bit */
				    8};  /* 8bit alpha, left to right, bottom to top */

	/* Set width and height */
	header[12] = (5 * SIZE_ORTHO + 4 * SIZE_DIAG)      & 0xff;
	header[13] = (5 * SIZE_ORTHO + 4 * SIZE_DIAG >> 8) & 0xff;
	header[14] = (samples * 5 * SIZE_ORTHO)      & 0xff;
	header[15] = (samples * 5 * SIZE_ORTHO >> 8) & 0xff;

	/* Write .tga header */
	fwrite(header, sizeof(unsigned char), sizeof(header) / sizeof(unsigned char), fp);

	/* Write pixel data  */
	for (int i = samples - 1; i >= 0; i--) {
		for (int y = 5 * SIZE_ORTHO - 1; y >= 0; y--) {
			for (int x = 0; x < 5 * SIZE_ORTHO; x++) {
				fputc(0, fp);                                          /* B */
				fputc((unsigned char)(ortho[i][y][x][1] * 255.0), fp); /* G */
				fputc((unsigned char)(ortho[i][y][x][0] * 255.0), fp); /* R */
				fputc(0, fp);                                          /* A */
			}

			for (int x = 0; x < 4 * SIZE_DIAG; x++) {
				if (i < 5) {
					fputc(0, fp);                                         /* B */
					fputc((unsigned char)(diag[i][y][x][1] * 255.0), fp); /* G */
					fputc((unsigned char)(diag[i][y][x][0] * 255.0), fp); /* R */
					fputc(0, fp);                                         /* A */
				}
				else {
					fputc(0, fp);
					fputc(0, fp);
					fputc(0, fp);
					fputc(0, fp);
				}
			}
		}
	}
}

static int generate_file(const char *path, bool subsampling, bool quantize, bool tga)
{
	FILE *fp = fopen(path, tga ? "wb" : "w");

	if (!fp) {
		fprintf(stderr, "Unable to open file: %s\n", path);
		return 1;
	}

	fprintf(stderr, "Generating %s\n", path);

	if (tga)
		write_tga(fp, subsampling);
	else
		write_csource(fp, subsampling, quantize);

	fclose(fp);

	return 0;
}

int main(int argc, char **argv)
{
	bool subsampling = false;
	bool quantize = false;
	bool tga = false;
	char *outfile;
	int status = 0;

	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "-s") == 0)
			subsampling = true;
		else if (strcmp(argv[i], "-q") == 0)
			quantize = true;
		else if (strcmp(argv[i], "-t") == 0)
			tga = true;
		else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			status = 1;
		}
	}

	if (status == 0 && argc > 1) {
		outfile = argv[argc - 1];
	}
	else {
		fprintf(stderr, "Usage: %s [OPTION]... OUTFILE\n", argv[0]);
		fprintf(stderr, "Options: -s  Calculate data for subpixel rendering\n");
		fprintf(stderr, "         -q  Quantize data to 256 levels\n");
		fprintf(stderr, "         -t  Write .tga file instead of C/C++ source\n");
		return 1;
	}

	/* Calculate areatex data */
	for (int i = 0; i < (subsampling ? 7 : 1); i++)
		areatex_ortho(i);

	for (int i = 0; i < (subsampling ? 5 : 1); i++)
		areatex_diag(i);

	/* Generate C++ source file or .tga file */
	return generate_file(outfile, subsampling, quantize, tga);
}

/* smaa_areatex.cpp ends here */
