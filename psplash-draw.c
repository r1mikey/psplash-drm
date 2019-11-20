/*
 *  pslash - a lightweight framebuffer splashscreen for embedded devices.
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  Parts of this file ( fifo handling ) based on 'usplash' copyright
 *  Matthew Garret.
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

#include "psplash-draw.h"
#include "psplash-config.h"
#include "psplash-colors.h"
#include "psplash-poky-img.h"
#include "psplash-bar-img.h"
#include "psplash-font.h"
#include "radeon-font.h"
#include "psplash-scanout.h"

#define SPLIT_LINE_POS(so)                            \
	(  (so)->height                                     \
	 - ((  PSPLASH_IMG_SPLIT_DENOMINATOR                \
	     - PSPLASH_IMG_SPLIT_NUMERATOR)                 \
	    * (so)->height / PSPLASH_IMG_SPLIT_DENOMINATOR) \
	)

static void psplash_draw_msg_one(PSplashScanout *so, const char *msg)
{
  int w, h;

  psplash_scanout_text_size (&w, &h, &radeon_font, msg);

  DBG("displaying '%s' %ix%i\n", msg, w, h);

  /* Clear */

  psplash_scanout_draw_rect (so,
			0,
			SPLIT_LINE_POS(so) - h,
			so->width,
			h,
			PSPLASH_BACKGROUND_COLOR);

  psplash_scanout_draw_text (so,
			(so->width-w)/2,
			SPLIT_LINE_POS(so) - h,
			PSPLASH_TEXT_COLOR,
			&radeon_font,
			msg);
}

void psplash_draw_msg(PSplashScanout *so, const char *msg)
{
  while (msg && so) {
    psplash_draw_msg_one(so, msg);
    so = so->next;
  }
}

static void psplash_draw_progress_one(PSplashScanout *so, int value)
{
  int x, y, width, height, barwidth;

  /* 4 pix border */
  x      = ((so->width  - BAR_IMG_WIDTH)/2) + 4 ;
  y      = SPLIT_LINE_POS(so) + 4;
  width  = BAR_IMG_WIDTH - 8;
  height = BAR_IMG_HEIGHT - 8;

  if (value > 0)
    {
      barwidth = (CLAMP(value,0,100) * width) / 100;
      psplash_scanout_draw_rect (so, x + barwidth, y,
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_scanout_draw_rect (so, x, y, barwidth,
			    height, PSPLASH_BAR_COLOR);
    }
  else
    {
      barwidth = (CLAMP(-value,0,100) * width) / 100;
      psplash_scanout_draw_rect (so, x, y,
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_scanout_draw_rect (so, x + width - barwidth,
			    y, barwidth, height,
			    PSPLASH_BAR_COLOR);
    }

  DBG("value: %i, width: %i, barwidth :%i\n", value,
		width, barwidth);
}

void psplash_draw_progress(PSplashScanout *so, int value)
{
  while (so) {
    psplash_draw_progress_one(so, value);
    so = so->next;
  }
}

static void psplash_draw_initial_one(PSplashScanout *so)
{
  /* Clear the background with #ecece1 */
  psplash_scanout_draw_rect (so, 0, 0, so->width, so->height,
                        PSPLASH_BACKGROUND_COLOR);

  /* Draw the Poky logo  */
  psplash_scanout_draw_image (so,
			 (so->width  - POKY_IMG_WIDTH)/2,
#if PSPLASH_IMG_FULLSCREEN
			 (so->height - POKY_IMG_HEIGHT)/2,
#else
			 (so->height * PSPLASH_IMG_SPLIT_NUMERATOR
			  / PSPLASH_IMG_SPLIT_DENOMINATOR - POKY_IMG_HEIGHT)/2,
#endif
			 POKY_IMG_WIDTH,
			 POKY_IMG_HEIGHT,
			 POKY_IMG_BYTES_PER_PIXEL,
			 POKY_IMG_ROWSTRIDE,
			 POKY_IMG_RLE_PIXEL_DATA);

  /* Draw progress bar border */
  psplash_scanout_draw_image (so,
			 (so->width  - BAR_IMG_WIDTH)/2,
			 SPLIT_LINE_POS(so),
			 BAR_IMG_WIDTH,
			 BAR_IMG_HEIGHT,
			 BAR_IMG_BYTES_PER_PIXEL,
			 BAR_IMG_ROWSTRIDE,
			 BAR_IMG_RLE_PIXEL_DATA);

  psplash_draw_progress_one(so, 0);

#ifdef PSPLASH_STARTUP_MSG
  psplash_draw_msg_one(so, PSPLASH_STARTUP_MSG);
#endif
}

void psplash_draw_initial(PSplashScanout *so)
{
  while (so) {
    psplash_draw_initial_one(so);
    so = so->next;
  }
}
