/* Hurd unionfs
   Copyright (C) 2001, 2002 Free Software Foundation, Inc.
   Written by Moritz Schulte <moritz@duesseldorf.ccc.de>.

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

/* This unionfs knows about two different kind of nodes: `light nodes'
   (in short: lnode) and `nodes' (or: netfs nodes) as used by
   libnetfs.  They have different tasks and therefore this division
   makes sense.

   lnodes form the filesystem tree as seen by the user; most
   importantly they contain the `name' of the node.

   lnodes are small and cheap, they are not cached (nodes are).

   lnodes are usually created when a node is looked up and destroyed
   when that node gets destroyed; but there are also reasons for
   lnodes _not_ being destroyed.

   The distinction makes it possible to keep certain information for
   the unionfs in these lnodes while netfs nodes don't have to stay in
   memory.  

   lnodes have to be looked up first before a node is looked up.  Each
   lnode contains a pointer to the netfs node, which might be NULL in
   case the netfs node is not in memory anymore.  */

/* General information and properties for the unionfs.  */

#ifndef INCLUDED_UNIONFS_H
#define INCLUDED_UNIONFS_H

#include <hurd/netfs.h>
#include <sys/types.h>

#include "node.h"
#include "lib.h"

/* Default maximum number of nodes in the cache.  */
#define NCACHE_SIZE 256

/* The inode for the root node.  */
#define UNIONFS_ROOT_INODE 1

/* Flags for UNIONFS_FLAGS.  */

/* Print debugging messages to stderr.  */
#define FLAG_UNIONFS_MODE_DEBUG 0x00000001
/* Use copy-on-write.  */
#define FLAG_UNIONFS_MODE_COW   0x00000002

/* Flags describing certain properties of the unionfs.  */
extern int unionfs_flags;

/* The filesystem id (the pid).  */
extern pid_t fsid;

/* Mapped time, used for updating node information.  */
extern volatile struct mapped_time_value *maptime;

/* A port to the underlying node.  */
extern mach_port_t underlying_node;

/* stat information for the underlying node.  */
extern struct stat underlying_node_stat;

/* Send a debugging message, if unionfs is in debugging mode.  */
#define debug_msg(fmt, args...)                          \
        do                                               \
          {                                              \
            if (unionfs_flags & FLAG_UNIONFS_MODE_DEBUG) \
              debug_msg_send (fmt , ## args);            \
          }                                              \
        while (0)

#endif
