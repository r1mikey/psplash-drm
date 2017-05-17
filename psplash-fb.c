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

void
psplash_fb_destroy (PSplashFB *fb)
{
  if (fb->fd >= 0)
    close (fb->fd);

  free(fb);
}

static int
attempt_to_change_pixel_format (PSplashFB *fb,
                                struct fb_var_screeninfo *fb_var)
{
  /* By default the framebuffer driver may have set an oversized
   * yres_virtual to support VT scrolling via the panning interface.
   *
   * We don't try and maintain this since it's more likely that we
   * will fail to increase the bpp if the driver's pre allocated
   * framebuffer isn't large enough.
   */
  fb_var->yres_virtual = fb_var->yres;

  /* First try setting an 8,8,8,0 pixel format so we don't have to do
   * any conversions while drawing. */

  fb_var->bits_per_pixel = 32;

  fb_var->red.offset = 0;
  fb_var->red.length = 8;

  fb_var->green.offset = 8;
  fb_var->green.length = 8;

  fb_var->blue.offset = 16;
  fb_var->blue.length = 8;

  fb_var->transp.offset = 0;
  fb_var->transp.length = 0;

  if (ioctl (fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0)
    {
      fprintf(stdout, "Switched to a 32 bpp 8,8,8 frame buffer\n");
      return 1;
    }
  else
    {
      fprintf(stderr,
              "Error, failed to switch to a 32 bpp 8,8,8 frame buffer\n");
    }

  /* Otherwise try a 16bpp 5,6,5 format */

  fb_var->bits_per_pixel = 16;

  fb_var->red.offset = 11;
  fb_var->red.length = 5;

  fb_var->green.offset = 5;
  fb_var->green.length = 6;

  fb_var->blue.offset = 0;
  fb_var->blue.length = 5;

  fb_var->transp.offset = 0;
  fb_var->transp.length = 0;

  if (ioctl (fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0)
    {
      fprintf(stdout, "Switched to a 16 bpp 5,6,5 frame buffer\n");
      return 1;
    }
  else
    {
      fprintf(stderr,
              "Error, failed to switch to a 16 bpp 5,6,5 frame buffer\n");
    }

  return 0;
}

PSplashFB*
psplash_fb_new (int angle, int fbdev_id)
{
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  int                      off;
  char                     fbdev[9] = "/dev/fb0";

  PSplashFB *fb = NULL;

  if (fbdev_id > 0 && fbdev_id < 10)
    {
        // Conversion from integer to ascii.
        fbdev[7] = fbdev_id + 48;
    }

  if ((fb = malloc (sizeof(PSplashFB))) == NULL)
    {
      perror ("Error no memory");
      goto fail;
    }

  memset (fb, 0, sizeof(PSplashFB));

  fb->fd = -1;

  if ((fb->fd = open (fbdev, O_RDWR)) < 0)
    {
      fprintf(stderr,
              "Error opening %s\n",
              fbdev);
      goto fail;
    }

  if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      perror ("Error getting variable framebuffer info");
      goto fail;
    }

  if (fb_var.bits_per_pixel < 16)
    {
      fprintf(stderr,
              "Error, no support currently for %i bpp frame buffers\n"
              "Trying to change pixel format...\n",
              fb_var.bits_per_pixel);
      if (!attempt_to_change_pixel_format (fb, &fb_var))
        goto fail;
    }

  if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      perror ("Error getting variable framebuffer info (2)");
      goto fail;
    }

  /* NB: It looks like the fbdev concept of fixed vs variable screen info is
   * broken. The line_length is part of the fixed info but it can be changed
   * if you set a new pixel format. */
  if (ioctl (fb->fd, FBIOGET_FSCREENINFO, &fb_fix) == -1)
    {
      perror ("Error getting fixed framebuffer info");
      goto fail;
    }

  fb->real_width  = fb->scanout.width  = fb_var.xres;
  fb->real_height = fb->scanout.height = fb_var.yres;
  fb->scanout.bpp    = fb_var.bits_per_pixel;
  fb->scanout.stride = fb_fix.line_length;
  fb->type   = fb_fix.type;
  fb->visual = fb_fix.visual;

  fb->scanout.red_offset = fb_var.red.offset;
  fb->scanout.red_length = fb_var.red.length;
  fb->scanout.green_offset = fb_var.green.offset;
  fb->scanout.green_length = fb_var.green.length;
  fb->scanout.blue_offset = fb_var.blue.offset;
  fb->scanout.blue_length = fb_var.blue.length;

  if (fb->scanout.red_offset == 11 && fb->scanout.red_length == 5 &&
      fb->scanout.green_offset == 5 && fb->scanout.green_length == 6 &&
      fb->scanout.blue_offset == 0 && fb->scanout.blue_length == 5) {
         fb->scanout.rgbmode = RGB565;
  } else if (fb->scanout.red_offset == 0 && fb->scanout.red_length == 5 &&
      fb->scanout.green_offset == 5 && fb->scanout.green_length == 6 &&
      fb->scanout.blue_offset == 11 && fb->scanout.blue_length == 5) {
         fb->scanout.rgbmode = BGR565;
  } else if (fb->scanout.red_offset == 16 && fb->scanout.red_length == 8 &&
      fb->scanout.green_offset == 8 && fb->scanout.green_length == 8 &&
      fb->scanout.blue_offset == 0 && fb->scanout.blue_length == 8) {
         fb->scanout.rgbmode = RGB888;
  } else if (fb->scanout.red_offset == 0 && fb->scanout.red_length == 8 &&
      fb->scanout.green_offset == 8 && fb->scanout.green_length == 8 &&
      fb->scanout.blue_offset == 16 && fb->scanout.blue_length == 8) {
         fb->scanout.rgbmode = BGR888;
  } else {
         fb->scanout.rgbmode = GENERIC;
  }

  DBG("width: %i, height: %i, bpp: %i, stride: %i",
      fb->scanout.width, fb->scanout.height, fb->scanout.bpp, fb->scanout.stride);

  fb->base = (char *) mmap ((caddr_t) NULL,
			    /*fb_fix.smem_len */
			    fb->scanout.stride * fb->scanout.height,
			    PROT_READ|PROT_WRITE,
			    MAP_SHARED,
			    fb->fd, 0);

  if (fb->base == (char *)-1)
    {
      perror("Error cannot mmap framebuffer ");
      goto fail;
    }

  off = (unsigned long) fb_fix.smem_start % (unsigned long) getpagesize();

  fb->scanout.data = fb->base + off;

#if 0
  /* FIXME: No support for 8pp as yet  */
  if (visual == FB_VISUAL_PSEUDOCOLOR
      || visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
  {
    static struct fb_cmap cmap;

    cmap.start = 0;
    cmap.len = 16;
    cmap.red = saved_red;
    cmap.green = saved_green;
    cmap.blue = saved_blue;
    cmap.transp = NULL;

    ioctl (fb, FBIOGETCMAP, &cmap);
  }

  if (!status)
    atexit (bogl_done);
  status = 2;
#endif

  fb->scanout.angle = angle;

  switch (fb->scanout.angle)
    {
    case 270:
    case 90:
      fb->scanout.width  = fb->real_height;
      fb->scanout.height = fb->real_width;
      break;
    case 180:
    case 0:
    default:
      break;
    }

  return fb;

 fail:

  if (fb)
    psplash_fb_destroy (fb);

  return NULL;
}
