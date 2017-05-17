/*
 *  pslash - a lightweight framebuffer splashscreen for embedded devices.
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_PSPLASH_SCANOUT_H
#define _HAVE_PSPLASH_SCANOUT_H

#include <stdint.h>

enum RGBMode {
    RGB565,
    BGR565,
    RGB888,
    BGR888,
    GENERIC,
};

typedef struct PSplashScanout
{
  struct PSplashScanout *next;
  int            width;
  int            height;
  int            bpp;
  int            stride;
  char		*data;

  int            angle;

  enum RGBMode   rgbmode;
  int            red_offset;
  int            red_length;
  int            green_offset;
  int            green_length;
  int            blue_offset;
  int            blue_length;
}
PSplashScanout;

extern void psplash_scanout_draw_rect(PSplashScanout *so, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
extern void psplash_scanout_draw_image(PSplashScanout *so, int x, int y, int img_width, int img_height, int img_bytes_per_pixel, int img_rowstride, uint8_t *rle_data);

typedef struct PSplashFont PSplashFont;
extern void psplash_scanout_text_size(int *width, int *height, const PSplashFont *font, const char *text);
extern void psplash_scanout_draw_text(PSplashScanout *so, int x, int y, uint8_t r, uint8_t g, uint8_t b, const PSplashFont *font, const char *text);

#endif
