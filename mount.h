/* Hurd unionmount
   General information and properties for unionmount/unionfs.

   Copyright (C) 2009 Free Software Foundation, Inc.

   Written by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef INCLUDED_MOUNT_H
#define INCLUDED_MOUNT_H

#include <error.h>
#include <unistd.h>
#include <hurd/hurd_types.h>

#include "node.h"

/* The command line for starting the mountee.  */
extern char * mountee_argz;
extern size_t mountee_argz_len;

extern mach_port_t mountee_root;
extern mach_port_t mountee_control;

extern int mountee_started;

/* Shows the mode in which the current instance of unionmount
   operates (transparent/non-transparent).  */
extern int transparent_mount;

/* Starts the mountee (given by `argz` and `argz_len`), attaches it to
   the node `np` and opens a port `port` to the mountee with
   `flags`.  */
error_t
start_mountee (node_t * np, char * argz, size_t argz_len, int flags,
	       mach_port_t * port);

/* Sets up a proxy node, sets the translator on it, and registers the
   filesystem published by the translator in the list of merged
   filesystems.  */
error_t
setup_unionmount (void);

#endif /* not INCLUDED_MOUNT_H */
