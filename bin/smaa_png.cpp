/*
 * Copyright 2002-2011 Guillaume Cottenceau and contributors.
 *           2014-2016 IRIE Shinsuke
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

/* smaa_png.cpp */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define PNG_DEBUG 3
#include <png.h>
#include <math.h>

#include "smaa.h"

void abort_(const char * s, ...)
{
	va_list args;
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	abort();
}

int width, height, rowbytes;
png_byte color_type;
png_byte bit_depth;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers;

void read_png_file(char* file_name)
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

	/* Expand any grayscale, RGB, or palette images to RGBA */
	png_set_expand(png_ptr);

	/* Reduce any 16-bits-per-sample images to 8-bits-per-sample */
	png_set_strip_16(png_ptr);

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[read_png_file] Error during read_image");

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);

	if (bit_depth == 16)
		rowbytes = width*8;
	else
		rowbytes = width*4;

	for (int y=0; y<height; y++)
		row_pointers[y] = (png_byte*) malloc(rowbytes);

	png_read_image(png_ptr, row_pointers);

	/* finish reading */
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fp);
}


void write_png_file(char* file_name)
{
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
		     8, 6, PNG_INTERLACE_NONE,
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

void process_file(void)
{
	using namespace SMAA;

	Image *orignImage, *edgesImage, *blendImage, *finalImage;
	float color[4], edges[4], weights[4];

	PixelShader ps(CONFIG_PRESET_HIGH);
	//ps.setEnableDiagDetection(false);
	//ps.setEnableCornerDetection(false);

	try {
		orignImage = new Image(width, height);
		edgesImage = new Image(width, height);
		blendImage = new Image(width, height);
		finalImage = new Image(width, height);
	}
	catch (ERROR_TYPE e) { abort_("Memory allocation failed"); }

	for (int y = 0; y < height; y++) {
		png_byte* ptr = row_pointers[y];
		for (int x = 0; x < width; x++) {
			color[0] = (float)*ptr++ / 255.0;
			color[1] = (float)*ptr++ / 255.0;
			color[2] = (float)*ptr++ / 255.0;
			color[3] = (float)*ptr++ / 255.0;
			orignImage->putPixel(x, y, color);
		}
	}

//	for (int i = 0; i < 10; i++) {

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ps.colorEdgeDetection(x, y, orignImage, NULL, edges);
			edgesImage->putPixel(x, y, edges);
		}
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ps.blendingWeightCalculation(x, y, edgesImage, NULL, weights);
			blendImage->putPixel(x, y, weights);
		}
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ps.neighborhoodBlending(x, y, orignImage, blendImage, color);
			finalImage->putPixel(x, y, color);
		}
	}

//	}

	for (int y = 0; y < height; y++) {
		png_byte* ptr = row_pointers[y];
		for (int x = 0; x < width; x++) {
			//orignImage->getPixel(x, y, color);
			//edgesImage->getPixel(x, y, color);
			//blendImage->getPixel(x, y, color);
			finalImage->getPixel(x, y, color);
			*ptr++ = (png_byte)roundf(color[0] * 255.0);
			*ptr++ = (png_byte)roundf(color[1] * 255.0);
			*ptr++ = (png_byte)roundf(color[2] * 255.0);
			*ptr++ = (png_byte)roundf(color[3] * 255.0);
		}
	}

	delete orignImage;
	delete edgesImage;
	delete blendImage;
	delete finalImage;
}

int main(int argc, char **argv)
{
	if (argc != 3)
		abort_("Usage: program_name <file_in> <file_out>");

	read_png_file(argv[1]);
	process_file();
	write_png_file(argv[2]);

	return 0;
}

/* smaa_png.cpp ends here */
