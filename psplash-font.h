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

#ifndef _HAVE_PSPLASH_FONT_H
#define _HAVE_PSPLASH_FONT_H

#include <stdint.h>

typedef struct PSplashFont
{
    char *name;				/* Font name. */
    int   height;			/* Height in pixels. */
    int   index_mask;			/* ((1 << N) - 1). */
    int  *offset;			/* (1 << N) offsets into index. */
    int  *index;
    uint32_t *content;
}
PSplashFont;

#endif
