#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define sign(x) ((x) >= 0.0 ? 1.0 : -1.0)

#define WIDTH 256
#define HEIGHT 256

typedef struct {
	double x, y;
} point_t;

typedef struct {
	point_t pts[2];
} line_t;

static uint8_t image[WIDTH * HEIGHT];

static void split_line(line_t line)
{
	double ox = line.pts[0].x; // x of origin point of the line
	double oy = line.pts[0].y; // y of origin point of the line
	double dx = line.pts[1].x - line.pts[0].x; // difference along x
	double dy = line.pts[1].y - line.pts[0].y; // difference along y

	point_t pts[1000];
	int pts_top = 0;

	assert(dx != 0.0);
	assert(dy != 0.0);

	if (dx >= 0.0) {
		if (dy >= 0.0) {
			double sx = 1.0 / dx; // step size along x
			double sy = 1.0 / dy; // step size along y
			double xt = (ceil(ox) - ox) / dx; // t of next vertical intersection
			double yt = (ceil(oy) - oy) / dy; // t of next horizontal intersection
			while (xt <= 1.0 || yt <= 1.0) {
				double t;
				if (xt == yt) {
					t = xt;
					xt += sx;
					yt += sy;
				} else if (xt < yt) {
					t = xt;
					xt += sx;
				} else {
					t = yt;
					yt += sy;
				}
				point_t pt = { ox + t * dx, oy + t * dy };
				pts[pts_top++] = pt;
			}
		} else {
		}
	} else {
		if (dy >= 0.0) {
		} else {
			double sx = -1.0 / dx; // step size along x
			double sy = -1.0 / dy; // step size along y
			double xt = (floor(ox) - ox) / dx; // t of next vertical intersection
			double yt = (floor(oy) - oy) / dy; // t of next horizontal intersection
			while (xt <= 1.0 || yt <= 1.0) {
				double t;
				if (xt == yt) {
					t = xt;
					xt += sx;
					yt += sy;
				} else if (xt < yt) {
					t = xt;
					xt += sx;
				} else {
					t = yt;
					yt += sy;
				}
				point_t pt = { ox + t * dx, oy + t * dy };
				pts[pts_top++] = pt;
			}
		}
	}

	for (int i = 0; i < pts_top; ++i) {
		point_t pt = pts[i];
		printf("f(t) = (%f, %f)\n", pt.x, pt.y);
	}
}

static void fmt_le_dword(char *buf, uint32_t v)
{
	buf[0] = v & 0xFF;
	buf[1] = v >> 8 & 0xFF;
	buf[2] = v >> 16 & 0xFF;
	buf[3] = v >> 24 & 0xFF;
}

static void write_bmp(void)
{
	char hdr[54] = { 0 };
	// Header
	hdr[0] = 'B';
	hdr[1] = 'M';
	fmt_le_dword(&hdr[2], 54 + 3 * WIDTH * HEIGHT); // size of file
	hdr[10] = 54; // offset to image data
	// InfoHeader
	hdr[14] = 40; // size of InfoHeader
	fmt_le_dword(&hdr[18], WIDTH);
	fmt_le_dword(&hdr[22], HEIGHT);
	hdr[26] = 1; // color planes
	hdr[28] = 24; // bpp
	fwrite(hdr, 1, 54, stdout);
	for (int y = 0; y < HEIGHT; ++y) {
		for (int x = 0; x < WIDTH; ++x) {
			unsigned char c = image[WIDTH * y + x];
			fputc(c, stdout); // r
			fputc(c, stdout); // g
			fputc(c, stdout); // b
		}
	}
}

int main(int argc, char const *argv[])
{
	line_t line = { { { 182.3, 101.4 }, { 5.1, 3.2 } } };
	split_line(line);
	// write_bmp();
	return EXIT_SUCCESS;
}
