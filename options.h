/* Hurd unionfs
   Copyright (C) 2001, 2002, 2009 Free Software Foundation, Inc.

   Written by Moritz Schulte <moritz@duesseldorf.ccc.de>.

   Adapted for unionmount by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or * (at your option) any later version.
 
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

/* Argument parsing.  */

/* The possible short options.  */
#define OPT_UNDERLYING 'u'
#define OPT_WRITABLE   'w'
#define OPT_DEBUG      'd'
#define OPT_CACHE_SIZE 'c'
#define OPT_REMOVE     'r'
#define OPT_ADD        'a'
#define OPT_PATTERN    'm'
#define OPT_PRIORITY   'p'
#define OPT_STOW       's'
#define OPT_MOUNT      't'
#define OPT_NOMOUNT    'n'

/* The long options.  */
#define OPT_LONG_UNDERLYING "underlying"
#define OPT_LONG_WRITABLE   "writable"
#define OPT_LONG_DEBUG      "debug"
#define OPT_LONG_CACHE_SIZE "cache-size"
#define OPT_LONG_REMOVE     "remove"
#define OPT_LONG_ADD        "add"
#define OPT_LONG_PATTERN    "match"
#define OPT_LONG_PRIORITY   "priority"
#define OPT_LONG_STOW       "stow"
#define OPT_LONG_MOUNT      "mount"
#define OPT_LONG_NOMOUNT    "no-mount"

#define OPT_LONG(o) "--" o

/* The final argp parser for runtime arguments.  */
extern struct argp argp_startup;

/* The final argp parser for startup arguments.  */
extern struct argp argp_runtime;

#define ULFS_MODE_ADD    0
#define ULFS_MODE_REMOVE 1
