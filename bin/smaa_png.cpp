/*
 * Copyright 2002-2011 Guillaume Cottenceau and contributors.
 *           2014-2016 IRIE Shinsuke
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

/* smaa_png.cpp */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <chrono>

#define PNG_DEBUG 3
#include <png.h>
#include <math.h>

#include "smaa.h"

static const float FLOAT_VAL_NOT_SPECIFIED = -1.0f;
static const int INT_VAL_NOT_SPECIFIED = -2;
static const int END_OF_LIST = -1;

enum edge_detection {
	ED_LUMA,
	ED_COLOR,
	ED_DEPTH,
};

static void abort_(const char * s, ...)
{
	va_list args;
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	abort();
}

static int width, height, rowbytes;
static png_byte color_type;
static png_byte bit_depth;
static bool has_alpha;

static png_structp png_ptr;
static png_infop info_ptr;
static int number_of_passes;
static png_bytep *row_pointers;


struct item {
	int id;
	char name[12];
};

static const struct item color_types[6] = {
	{PNG_COLOR_TYPE_GRAY,       "GRAY"},
	{PNG_COLOR_TYPE_GRAY_ALPHA, "GRAY_ALPHA"},
	{PNG_COLOR_TYPE_PALETTE,    "PALETTE"},
	{PNG_COLOR_TYPE_RGB,        "RGB"},
	{PNG_COLOR_TYPE_RGB_ALPHA,  "RGB_ALPHA"},
	{END_OF_LIST, ""}
};

static const struct item edge_detection_types[4] = {
	{ED_LUMA,  "luma"},
	{ED_COLOR, "color"},
	{ED_DEPTH, "depth"},
	{END_OF_LIST, ""}
};

static const struct item config_presets[6] = {
	{SMAA::CONFIG_PRESET_LOW,     "low"},
	{SMAA::CONFIG_PRESET_MEDIUM,  "medium"},
	{SMAA::CONFIG_PRESET_HIGH,    "high"},
	{SMAA::CONFIG_PRESET_ULTRA,   "ultra"},
	{SMAA::CONFIG_PRESET_EXTREME, "extreme"},
	{END_OF_LIST, ""}
};

static const char *assoc(int key, const struct item *list)
{
	int i = 0;
	while(list[i].id != END_OF_LIST) {
		if (key == list[i].id)
			return list[i].name;
		i++;
	}
	return NULL;
}

static int rassoc(const char *key, const struct item *list)
{
	int i = 0;
	while(list[i].id != END_OF_LIST) {
		if (strcmp(key, list[i].name) == 0)
			return list[i].id;
		i++;
	}
	return END_OF_LIST;
}

static int check_png_filename(const char *file_name)
{
	const char *extension = strrchr(file_name, '.');

	if (!extension)
		fprintf(stderr, "File name has no extension: %s\n", file_name);
	else if (strcmp(extension, ".png") != 0 && strcmp(extension, ".PNG") != 0)
		fprintf(stderr, "File extension is not \".png\": %s\n", file_name);
	else
		return 0;

	return 1;
}

static void print_png_info(const char *file_name, const char *inout_label)
{
	const char *type_name;

	fprintf(stderr, "%s file: %s\n", inout_label, file_name);
	fprintf(stderr, "  width x height: %d x %d\n", width, height);
	fprintf(stderr, "  color type: %s\n", assoc(color_type, color_types));
	fprintf(stderr, "  alpha channel or tRNS chanks: %s\n", has_alpha ? "yes" : "no");
	fprintf(stderr, "  bit depth: %d%s\n", bit_depth, (bit_depth < 8) ? " (expanded to 8bit)" : "");
}

static void read_png_file(const char *file_name, bool print_info)
{
	unsigned char header[8];    // 8 is the maximum size that can be checked

	/* open file and test for it being a png */
	FILE *fp = fopen(file_name, "rb");
	if (!fp)
		abort_("[read_png_file] File %s could not be opened for reading", file_name);
	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8))
		abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);


	/* initialize stuff */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		abort_("[read_png_file] png_create_read_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		abort_("[read_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[read_png_file] Error during init_io");

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	/* is there transparency data? */
	if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GA)
		has_alpha = true;
	else {
		png_bytep trans = NULL;
		int num_trans = 0;
		png_color_16p trans_values = NULL;

		png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trans_values);
		has_alpha = (trans != NULL && num_trans > 0 || trans_values != NULL);
	}

	/* print information of input image */
	if (print_info)
		print_png_info(file_name, "input");

	/* Expand any grayscale or palette images to RGB */
	png_set_expand(png_ptr);

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	color_type = has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
	bit_depth = (bit_depth < 8) ? 8 : bit_depth;

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[read_png_file] Error during read_image");

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);

	if (bit_depth == 16)
		rowbytes = width * (has_alpha ? 8 : 6);
	else
		rowbytes = width * (has_alpha ? 4 : 3);

	for (int y=0; y<height; y++)
		row_pointers[y] = (png_byte*) malloc(rowbytes);

	png_read_image(png_ptr, row_pointers);

	/* finish reading */
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fp);
}


static void write_png_file(const char *file_name, bool print_info)
{
	/* print information of output image */
	if (print_info)
		print_png_info(file_name, "output");

	/* create file */
	FILE *fp = fopen(file_name, "wb");
	if (!fp)
		abort_("[write_png_file] File %s could not be opened for writing", file_name);


	/* initialize stuff */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		abort_("[write_png_file] png_create_write_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		abort_("[write_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[write_png_file] Error during init_io");

	png_init_io(png_ptr, fp);


	/* write header */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[write_png_file] Error during writing header");

	png_set_IHDR(png_ptr, info_ptr, width, height,
		     bit_depth, color_type, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[write_png_file] Error during writing bytes");

	png_write_image(png_ptr, row_pointers);


	/* finish writing */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[write_png_file] Error during end of write");

	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	/* cleanup heap allocation */
	for (int y=0; y<height; y++)
		free(row_pointers[y]);
	free(row_pointers);
}

static inline void write_pixel16(png_byte **ptr, float color)
{
	unsigned int c = (unsigned int)roundf(color * 65535.0f);
	*(*ptr)++ = (png_byte)(c >> 8);
	*(*ptr)++ = (png_byte)(c & 0xff);
}

static void process_file(int preset, int detection_type, float threshold, float adaptation,
		  int ortho_steps, int diag_steps, int rounding, bool print_info)
{
	using namespace SMAA;
	using namespace std::chrono;

	Image *orignImage, *edgesImage, *blendImage, *finalImage, *depthImage;
	float color[4], edges[4], weights[4], depth[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const char *type_name;
	steady_clock::time_point begin, end;

	/* setup SMAA pixel shader */
	PixelShader ps(preset);
	if (threshold != FLOAT_VAL_NOT_SPECIFIED)
		ps.setThreshold(threshold);
	if (adaptation != FLOAT_VAL_NOT_SPECIFIED)
		ps.setLocalContrastAdaptationFactor(adaptation);
	if (ortho_steps != INT_VAL_NOT_SPECIFIED)
		ps.setMaxSearchSteps(ortho_steps);
	if (diag_steps != INT_VAL_NOT_SPECIFIED) {
		if (diag_steps != -1) {
			ps.setEnableDiagDetection(true);
			ps.setMaxSearchStepsDiag(diag_steps);
		}
		else
			ps.setEnableDiagDetection(false);
	}
	if (rounding != INT_VAL_NOT_SPECIFIED) {
		if (rounding != -1) {
			ps.setEnableCornerDetection(true);
			ps.setCornerRounding(rounding);
		}
		else
			ps.setEnableCornerDetection(false);
	}

	if (print_info) {
		fprintf(stderr, "\n");
		fprintf(stderr, "edge detection type: %s\n", assoc(detection_type, edge_detection_types));
		fprintf(stderr, "  threshold: %f\n",
			(detection_type != ED_DEPTH) ? ps.getThreshold() : ps.getDepthThreshold());
		fprintf(stderr, "  predicated thresholding: off (not supported)\n");
		fprintf(stderr, "  local contrast adaptation factor: %f\n", ps.getLocalContrastAdaptationFactor());
		fprintf(stderr, "\n");
		fprintf(stderr, "maximum search steps: %d\n", ps.getMaxSearchSteps());
		fprintf(stderr, "diagonal search: %s\n", ps.getEnableDiagDetection() ? "on" : "off");
		if (ps.getEnableDiagDetection())
			fprintf(stderr, "  maximum diagonal search steps: %d\n", ps.getMaxSearchStepsDiag());
		fprintf(stderr, "corner processing: %s\n", ps.getEnableCornerDetection() ? "on" : "off");
		if (ps.getEnableCornerDetection())
			fprintf(stderr, "  corner rounding: %d\n", ps.getCornerRounding());
		fprintf(stderr, "\n");
	}

	/* prepare image buffers */
	try {
		orignImage = new Image(width, height);
		edgesImage = new Image(width, height);
		blendImage = new Image(width, height);
		finalImage = new Image(width, height);
		if (detection_type == ED_DEPTH)
			depthImage = new Image(width, height);
	}
	catch (ERROR_TYPE e) { abort_("Memory allocation failed"); }

	/* read from png buffer */
	for (int y = 0; y < height; y++) {
		png_byte* ptr = row_pointers[y];
		for (int x = 0; x < width; x++) {
			if (bit_depth == 16) {
				color[0] = (float)((*ptr++ << 8) + *ptr++) / 65535.0f;
				color[1] = (float)((*ptr++ << 8) + *ptr++) / 65535.0f;
				color[2] = (float)((*ptr++ << 8) + *ptr++) / 65535.0f;
				color[3] = has_alpha ? (float)((*ptr++ << 8) + *ptr++) / 65535.0f : 1.0f;
			}
			else {
				color[0] = (float)*ptr++ / 255.0f;
				color[1] = (float)*ptr++ / 255.0f;
				color[2] = (float)*ptr++ / 255.0f;
				color[3] = has_alpha ? (float)*ptr++ / 255.0f : 1.0f;
			}

			if (detection_type == ED_DEPTH) {
				depth[0] = color[3];
				depthImage->putPixel(x, y, depth);
				color[3] = 1.0f;
			}

			orignImage->putPixel(x, y, color);
		}
	}

	if (detection_type == ED_DEPTH) {
		/* alpha channel was consumed as depth */
		color_type = PNG_COLOR_TYPE_RGB;
		has_alpha = false;
	}

	/* record starting time to calculate elapsed time */
	if (print_info)
		begin = steady_clock::now();

	/* do anti-aliasing (3 passes) */
	/* 1. edge detection */
	switch (detection_type) {
		case ED_LUMA:
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					ps.lumaEdgeDetection(x, y, orignImage, NULL, edges);
					edgesImage->putPixel(x, y, edges);
				}
			}
			break;
		case ED_COLOR:
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					ps.colorEdgeDetection(x, y, orignImage, NULL, edges);
					edgesImage->putPixel(x, y, edges);
				}
			}
			break;
		case ED_DEPTH:
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					ps.depthEdgeDetection(x, y, depthImage, edges);
					edgesImage->putPixel(x, y, edges);
				}
			}
			break;
	}

	/* 2. calculate blending weights */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ps.blendingWeightCalculation(x, y, edgesImage, NULL, weights);
			blendImage->putPixel(x, y, weights);
		}
	}

	/* 3. blend color with neighboring pixels */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ps.neighborhoodBlending(x, y, orignImage, blendImage, NULL, color);
			finalImage->putPixel(x, y, color);
		}
	}

	/* print elapsed time */
	if (print_info) {
		end = steady_clock::now();
		long int elapsed_time = duration_cast<milliseconds>(end - begin).count();
		fprintf(stderr, "elapsed time: %ld ms\n\n", elapsed_time);
	}

	/* write back to png buffer */
	for (int y = 0; y < height; y++) {
		png_byte* ptr = row_pointers[y];
		for (int x = 0; x < width; x++) {
			//orignImage->getPixel(x, y, color);
			//edgesImage->getPixel(x, y, color);
			//blendImage->getPixel(x, y, color);
			finalImage->getPixel(x, y, color);
			if (bit_depth == 16) {
				write_pixel16(&ptr, color[0]);
				write_pixel16(&ptr, color[1]);
				write_pixel16(&ptr, color[2]);
				if (has_alpha)
					write_pixel16(&ptr, color[3]);
			}
			else {
				*ptr++ = (png_byte)roundf(color[0] * 255.0f);
				*ptr++ = (png_byte)roundf(color[1] * 255.0f);
				*ptr++ = (png_byte)roundf(color[2] * 255.0f);
				if (has_alpha)
					*ptr++ = (png_byte)roundf(color[3] * 255.0f);
			}
		}
	}

	/* delete image buffers */
	delete orignImage;
	delete edgesImage;
	delete blendImage;
	delete finalImage;
	if (detection_type == ED_DEPTH)
		delete depthImage;
}

int main(int argc, char **argv)
{
	int preset = SMAA::CONFIG_PRESET_HIGH;
	int detection = ED_COLOR;
	float threshold = FLOAT_VAL_NOT_SPECIFIED;
	float adaptation = FLOAT_VAL_NOT_SPECIFIED;
	int ortho_steps = INT_VAL_NOT_SPECIFIED;
	int diag_steps = INT_VAL_NOT_SPECIFIED;
	int rounding = INT_VAL_NOT_SPECIFIED;
	bool verbose = false;
	bool help = false;
	char *infile = NULL;
	char *outfile = NULL;
	int status = 0;

	for (int i = 1; i < argc; i++) {
		char *ptr = argv[i];
		if (*ptr++ == '-' && *ptr != '\0') {
			char c, *optarg, *endptr;
			while ((c = *ptr++) != '\0') {
				if (strchr("petasdc", c)) {
					if (*ptr != '\0')
						optarg = ptr;
					else if (++i < argc)
						optarg = argv[i];
					else {
						fprintf(stderr, "Option -%c requires an argument.\n", c);
						status = 1;
						break;
					}

					if (c == 'p') {
						preset = rassoc(optarg, config_presets);
						if (preset == END_OF_LIST) {
							fprintf(stderr, "Unknown preset name: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 'e') {
						detection = rassoc(optarg, edge_detection_types);
						if (detection == END_OF_LIST) {
							fprintf(stderr, "Unknown detection type: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 't') {
						threshold = strtof(optarg, &endptr);
						if (threshold < 0.0f || *endptr != '\0') {
							fprintf(stderr, "Invalid threshold: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 'a') {
						adaptation = strtof(optarg, &endptr);
						if (adaptation < 0.0f || *endptr != '\0') {
							fprintf(stderr, "Invalid contrast adaptation factor: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 's') {
						ortho_steps = strtol(optarg, &endptr, 0);
						if (ortho_steps < 0 || *endptr != '\0') {
							fprintf(stderr, "Invalid maximum search steps: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 'd') {
						diag_steps = strtol(optarg, &endptr, 0);
						if (diag_steps < -1 || *endptr != '\0') { /* -1 means disable the processing */
							fprintf(stderr, "Invalid maximum diagonal search steps: %s\n", optarg);
							status = 1;
						}
					}
					else if (c == 'c') {
						rounding = strtol(optarg, &endptr, 0);
						if (rounding < -1 || *endptr != '\0') { /* -1 means disable the processing */
							fprintf(stderr, "Invalid corner rounding: %s\n", optarg);
							status = 1;
						}
					}

					break;
				}
				else if (c == 'v')
					verbose = true;
				else if (c == 'h')
					help = true;
				else {
					fprintf(stderr, "Unknown option: -%c\n", c);
					status = 1;
					break;
				}
			}
		}
		else if (outfile) {
			fprintf(stderr, "Too much file names: %s, %s, %s\n", infile, outfile, argv[i]);
			status = 1;
		}
		else if (infile)
			outfile = argv[i];
		else
			infile = argv[i];

		if (status != 0)
			break;
	}

	if (status == 0 && !help && !outfile) {
		fprintf(stderr, "Two file names are required.\n");
		status = 1;
	}

	if (status != 0 || help) {
		if (status != 0)
			fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s [OPTION]... INFILE OUTFILE\n", argv[0]);
		fprintf(stderr, "Remove jaggies from PNG image and write antialiased PNG image.\n\n");
		fprintf(stderr, "  -p PRESET     Specify base configuration preset\n");
		fprintf(stderr, "                                                 [low|medium|high|ultra|extreme]\n");
		fprintf(stderr, "  -e DETECTTYPE Specify edge detection type                   [luma|color|depth]\n");
		fprintf(stderr, "                (Depth edge detection uses alpha channel as depths)\n");
		fprintf(stderr, "  -t THRESHOLD  Specify threshold of edge detection                   [0.0, 5.0]\n");
		fprintf(stderr, "  -a FACTOR     Specify local contrast adaptation factor              [1.0, inf]\n");
		fprintf(stderr, "  -s STEPS      Specify maximum search steps                            [1, 362]\n");
		fprintf(stderr, "  -d STEPS      Specify maximum diagonal search steps\n");
		fprintf(stderr, "                (-1 means disable diagonal processing)             -1 or [1, 19]\n");
		fprintf(stderr, "  -c ROUNDING   Specify corner rounding\n");
		fprintf(stderr, "                (-1 means disable corner processing)              -1 or [0, 100]\n");
		fprintf(stderr, "  -v            Print details of what is being done\n");
		fprintf(stderr, "  -h            Print this help and exit\n");
		return status;
	}

	if (verbose)
		fprintf(stderr, "smaa_png version %s\n\n", SMAA::VERSION);

	read_png_file(infile, verbose);
	process_file(preset, detection, threshold, adaptation, ortho_steps, diag_steps, rounding, verbose);
	write_png_file(outfile, verbose);

	if (verbose)
		fprintf(stderr, "\ndone.\n");

	return 0;
}

/* smaa_png.cpp ends here */
