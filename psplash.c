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

void psplash_exit(int UNUSED(signum))
{
  DBG("mark");
  psplash_console_reset();
}

static int main_fbdev(int argc, char** argv)
{
  int        i = 0, angle = 0, fbdev_id = 0, ret = 0;
  PSplashFB *fb;
  bool       disable_console_switch = FALSE;

  signal(SIGHUP, psplash_exit);
  signal(SIGINT, psplash_exit);
  signal(SIGQUIT, psplash_exit);

  while (++i < argc) {
    if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--no-console-switch"))
      {
        disable_console_switch = TRUE;
        continue;
      }

    if (!strcmp(argv[i],"-a") || !strcmp(argv[i],"--angle"))
      {
        if (++i >= argc) goto fail;
        angle = atoi(argv[i]);
        continue;
      }

    if (!strcmp(argv[i],"-f") || !strcmp(argv[i],"--fbdev"))
      {
        if (++i >= argc) goto fail;
        fbdev_id = atoi(argv[i]);
        continue;
      }

    fail:
      fprintf(stderr,
              "Usage: %s [-n|--no-console-switch][-a|--angle <0|90|180|270>][-f|--fbdev <0..9>]\n",
              argv[0]);
      exit(-1);
  }

  if (!disable_console_switch)
    psplash_console_switch ();

  if ((fb = psplash_fb_new(angle,fbdev_id)) == NULL)
    {
	  ret = -1;
	  goto fb_fail;
    }

  psplash_main (&fb->scanout);

  psplash_fb_destroy (fb);

 fb_fail:

  if (!disable_console_switch)
    psplash_console_reset ();

  return ret;
}


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

  ret = main_fbdev(argc, argv);

  unlink(PSPLASH_FIFO);
  return ret;
}
