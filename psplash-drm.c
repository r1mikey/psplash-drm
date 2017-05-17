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

#include "psplash.h"
#include "psplash-main.h"
#include "psplash-config.h"
#include "psplash-colors.h"
#include "psplash-poky-img.h"
#include "psplash-bar-img.h"
#include "psplash-draw.h"
#include "psplash-drm.h"
#include "psplash-kms.h"

#include <stdint.h>

static int main_drm_inner(const char * dridev, struct drm_rotation_t * rotations)
{
  int ret;
  int fd;
  struct drm_scanout_t * scanouts;

  ret = 1;
  fd = -1;
  scanouts = NULL;

  if ((fd = open(dridev, O_RDWR)) < 0) {
    goto cleanup;
  }

  if (!(scanouts = find_drm_scanouts(fd, rotations)))
    goto cleanup;

  psplash_main(&scanouts->pso);
  ret = 0;

cleanup:
  if (fd != -1) {
    free_drm_scanouts(fd, scanouts);
    close(fd);
  }

  return ret;
}

int main_drm(int argc, char** argv)
{
  int ret;
  int i;
  uint32_t default_angle;
  unsigned int dridev_id;
  struct drm_rotation_t * rotations;
  char * rotation_spec;
  char path_buffer[512];
  char blank_spec[] = "";

  ret = 1;
  default_angle = 0;
  dridev_id = 0;
  rotations = NULL;
  rotation_spec = blank_spec;
  i = 0;

  while (++i < argc) {
    if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--angle")) {
      if (++i >= argc)
        goto cleanup;
      default_angle = atoi(argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--dridev")) {
      if (++i >= argc)
        goto cleanup;
      dridev_id = atoi(argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--rotation")) {
      if (++i >= argc)
        goto cleanup;
      rotation_spec = argv[i];
      continue;
    }
  }

  snprintf(path_buffer, 511, "/dev/dri/card%u", dridev_id);
  if (!(rotations = parse_rotation_string(rotation_spec, default_angle)))
    goto cleanup;

  ret = main_drm_inner(path_buffer, rotations);

cleanup:
  free_drm_rotations(rotations);
  return ret;
}
