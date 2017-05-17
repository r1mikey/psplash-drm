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
#include "psplash-fbdev.h"
#include "psplash-drm.h"

int main(int argc, char** argv)
{
  char *tmpdir;
  int ret;

  if (!(tmpdir = getenv("TMPDIR")))
    tmpdir = "/tmp";
  chdir(tmpdir);

  if (mkfifo(PSPLASH_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
    if (errno!=EEXIST) {
      perror("mkfifo");
      return 1;
    }
  }

#if defined(PSPLASH_FBDEV)
  ret = main_fbdev(argc, argv);
#else
#if defined(PSPLASH_DRM)
  ret = main_drm(argc, argv);
#else
  ret = 1;
#endif
#endif

  unlink(PSPLASH_FIFO);
  return ret;
}
