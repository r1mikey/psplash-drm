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

#include "psplash-main.h"
#include "psplash.h"
#include "psplash-config.h"
#include "psplash-colors.h"
#include "psplash-poky-img.h"
#include "psplash-bar-img.h"
#include "psplash-draw.h"

static int parse_command(PSplashScanout *so, char *string)
{
  char *command;

  DBG("got cmd %s", string);

  if (strcmp(string, "QUIT") == 0)
    return 1;

  command = strtok(string, " ");

  if (!strcmp(command,"PROGRESS"))
    psplash_draw_progress(so, atoi(strtok(NULL, "\0")));
  else if (!strcmp(command,"MSG"))
    psplash_draw_msg(so, strtok(NULL, "\0"));
  else if (!strcmp(command, "QUIT"))
    return 1;

  return 0;
}

void psplash_main(PSplashScanout *so)
{
  int            err;
  int            pipe_fd;
  ssize_t        length = 0;
  fd_set         descriptors;
  char          *end;
  char           command[2048];

  pipe_fd = open(PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
  if (pipe_fd==-1) {
    perror("pipe open");
    return;
  }

  psplash_draw_initial(so);

  FD_ZERO(&descriptors);
  FD_SET(pipe_fd, &descriptors);

  end = command;

  while (1)
    {
      err = select(pipe_fd+1, &descriptors, NULL, NULL, NULL);

      if (err <= 0)
	{
	  /*
	  if (errno == EINTR)
	    continue;
	  */
          DBG("select got err %d: %m", errno);
	  return;
	}

      length += read (pipe_fd, end, sizeof(command) - (end - command));

      if (length == 0)
	{
	  /* Reopen to see if there's anything more for us */
	  close(pipe_fd);
	  pipe_fd = open(PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
	  goto out;
	}

      if (command[length-1] == '\0')
	{
	  if (parse_command(so, command))
	    return;
	  length = 0;
	}
      else if (command[length-1] == '\n')
	{
	  command[length-1] = '\0';
	  if (parse_command(so, command))
	    return;
	  length = 0;
	}


    out:
      end = &command[length];

      FD_ZERO(&descriptors);
      FD_SET(pipe_fd,&descriptors);
    }

  return;
}
