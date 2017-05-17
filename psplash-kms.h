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

#ifndef _HAVE_PSPLASH_KMS_H
#define _HAVE_PSPLASH_KMS_H

#include "psplash-scanout.h"

#include <drm/drm_mode.h>

struct drm_rotation_t {
  unsigned int interface;
  unsigned int instance;
  unsigned int rotation;
  struct drm_rotation_t * next;
};

struct scanout_buffer_t {
  struct drm_mode_crtc saved_crtc;
  uint32_t saved_connector;
  struct drm_mode_create_dumb create_dumb;
  struct drm_mode_destroy_dumb destroy_dumb;
  struct drm_mode_fb_cmd cmd_dumb;
  void * fb_base;
  unsigned int interface;
  unsigned int instance;
  long fb_w;
  long fb_h;
  long fb_stride;
  long fb_bpp;
};

struct drm_scanout_t {
  PSplashScanout pso;
  struct drm_scanout_t * next;
  struct scanout_buffer_t * scanout;
};

extern struct drm_scanout_t * find_drm_scanouts(int fd, struct drm_rotation_t * rotations);
extern void free_drm_scanouts(int fd, struct drm_scanout_t * root);

extern struct drm_rotation_t * parse_rotation_string(
    char * arg, uint32_t default_rotation);
extern void free_drm_rotations(struct drm_rotation_t * root);

#endif
