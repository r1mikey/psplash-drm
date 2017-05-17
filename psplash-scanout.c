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

#include <endian.h>
#include "psplash.h"
#include "psplash-scanout.h"
#include "psplash-font.h"

#define OFFSET(so,x,y) (((y) * (so)->stride) + ((x) * ((so)->bpp >> 3)))

static inline void psplash_scanout_plot_pixel(PSplashScanout *so, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
  int off;

  if (x < 0 || x > so->width-1 || y < 0 || y > so->height-1)
    return;

  switch (so->angle)
    {
    case 270:
      off = OFFSET (so, so->height - y - 1, x);
      break;
    case 180:
      off = OFFSET (so, so->width - x - 1, so->height - y - 1);
      break;
    case 90:
      off = OFFSET (so, y, so->width - x - 1);
      break;
    case 0:
    default:
      off = OFFSET (so, x, y);
      break;
    }

  if (so->rgbmode == RGB565 || so->rgbmode == RGB888) {
    switch (so->bpp)
      {
      case 24:
#if __BYTE_ORDER == __BIG_ENDIAN
        *(so->data + off + 0) = r;
        *(so->data + off + 1) = g;
        *(so->data + off + 2) = b;
#else
        *(so->data + off + 0) = b;
        *(so->data + off + 1) = g;
        *(so->data + off + 2) = r;
#endif
        break;
      case 32:
        *(volatile uint32_t *) (so->data + off)
          = (r << 16) | (g << 8) | (b);
        break;

      case 16:
        *(volatile uint16_t *) (so->data + off)
	  = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        break;
      default:
        /* depth not supported yet */
        break;
      }
  } else if (so->rgbmode == BGR565 || so->rgbmode == BGR888) {
    switch (so->bpp)
      {
      case 24:
#if __BYTE_ORDER == __BIG_ENDIAN
        *(so->data + off + 0) = b;
        *(so->data + off + 1) = g;
        *(so->data + off + 2) = r;
#else
        *(so->data + off + 0) = r;
        *(so->data + off + 1) = g;
        *(so->data + off + 2) = b;
#endif
        break;
      case 32:
        *(volatile uint32_t *) (so->data + off)
          = (b << 16) | (g << 8) | (r);
        break;
      case 16:
        *(volatile uint16_t *) (so->data + off)
	  = ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
        break;
      default:
        /* depth not supported yet */
        break;
      }
  } else {
    switch (so->bpp)
      {
      case 32:
        *(volatile uint32_t *) (so->data + off)
	  = ((r >> (8 - so->red_length)) << so->red_offset)
	      | ((g >> (8 - so->green_length)) << so->green_offset)
	      | ((b >> (8 - so->blue_length)) << so->blue_offset);
        break;
      case 16:
        *(volatile uint16_t *) (so->data + off)
	  = ((r >> (8 - so->red_length)) << so->red_offset)
	      | ((g >> (8 - so->green_length)) << so->green_offset)
	      | ((b >> (8 - so->blue_length)) << so->blue_offset);
        break;
      default:
        /* depth not supported yet */
        break;
      }
  }
}

void psplash_scanout_draw_rect(PSplashScanout *so, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
  int dx, dy;

  for (dy=0; dy < h; dy++)
    for (dx=0; dx < w; dx++)
	psplash_scanout_plot_pixel (so, x+dx, y+dy, r, g, b);
}

void psplash_scanout_draw_image(PSplashScanout *so, int x, int y, int img_width, int img_height, int img_bytes_per_pixel, int img_rowstride, uint8_t *rle_data)
{
  uint8_t     *p = rle_data;
  int          dx = 0, dy = 0,  total_len;
  unsigned int len;

  total_len = img_rowstride * img_height;

  /* FIXME: Optimise, check for over runs ... */
  while ((p - rle_data) < total_len)
    {
      len = *(p++);

      if (len & 128)
	{
	  len -= 128;

	  if (len == 0) break;

	  do
	    {
	      if ((img_bytes_per_pixel < 4 || *(p+3)) && dx < img_width)
	        psplash_scanout_plot_pixel (so, x+dx, y+dy, *(p), *(p+1), *(p+2));
	      if (++dx * img_bytes_per_pixel >= img_rowstride) { dx=0; dy++; }
	    }
	  while (--len);

	  p += img_bytes_per_pixel;
	}
      else
	{
	  if (len == 0) break;

	  do
	    {
	      if ((img_bytes_per_pixel < 4 || *(p+3)) && dx < img_width)
	        psplash_scanout_plot_pixel (so, x+dx, y+dy, *(p), *(p+1), *(p+2));
	      if (++dx * img_bytes_per_pixel >= img_rowstride) { dx=0; dy++; }
	      p += img_bytes_per_pixel;
	    }
	  while (--len && (p - rle_data) < total_len);
	}
    }
}

/* Font rendering code based on BOGL by Ben Pfaff */

static int
psplash_font_glyph(const PSplashFont *font, wchar_t wc, u_int32_t **bitmap)
{
  int mask = font->index_mask;
  int i;

  for (;;)
    {
      for (i = font->offset[wc & mask]; font->index[i]; i += 2)
	{
	  if ((wchar_t)(font->index[i] & ~mask) == (wc & ~mask))
	    {
	      if (bitmap != NULL)
		*bitmap = &font->content[font->index[i+1]];
	      return font->index[i] & mask;
	    }
	}
    }
  return 0;
}

void psplash_scanout_text_size(int *width, int *height, const PSplashFont *font, const char *text)
{
  char   *c = (char*)text;
  wchar_t wc;
  int     k, n, w, h, mw;

  n = strlen (text);
  mw = h = w = 0;

  mbtowc (0, 0, 0);
  for (; (k = mbtowc (&wc, c, n)) > 0; c += k, n -= k)
    {
      if (*c == '\n')
	{
	  if (w > mw)
	    mw = 0;
	  h += font->height;
	  continue;
	}

      w += psplash_font_glyph (font, wc, NULL);
    }

  *width  = (w > mw) ? w : mw;
  *height = (h == 0) ? font->height : h;
}

void psplash_scanout_draw_text(PSplashScanout *so, int x, int y, uint8_t r, uint8_t g, uint8_t b, const PSplashFont *font, const char *text)
{
  int h, w, k, n, cx, cy, dx, dy;
  char *c = (char*)text;
  wchar_t wc;

  n = strlen (text);
  h = font->height;
  dx = dy = 0;

  mbtowc (0, 0, 0);
  for (; (k = mbtowc(&wc, c, n)) > 0; c += k, n -= k) {
    u_int32_t *glyph = NULL;

    if (*c == '\n') {
      dy += h;
      dx  = 0;
      continue;
    }

    w = psplash_font_glyph(font, wc, &glyph);
    if (glyph == NULL)
      continue;

    for (cy = 0; cy < h; cy++) {
      u_int32_t g_ = *glyph++;

      for (cx = 0; cx < w; cx++) {
        if (g_ & 0x80000000)
          psplash_scanout_plot_pixel (so, x+dx+cx, y+dy+cy, r, g, b);
        g_ <<= 1;
      }
    }

    dx += w;
  }
}
