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

#ifndef INCLUDED_NCACHE_H
#define INCLUDED_NCACHE_H

#include <error.h>
#include <hurd/netfs.h>

#include "node.h"

typedef struct ncache
{
  node_t *mru; 		        /* Reference to the mru end of the
				   cache chain.  */
  node_t *lru;		        /* Reference to the lru end of the
				   cache chain.  */
  int size_max;			/* Maximal number of nodes to
				   cache.  */
  int size_current;		/* Current number of nodes in the
				   cache.  */
  struct mutex lock;		/* A lock.  */
} ncache_t;

/* Cache size, may be overwritten by the user.  */
extern int ncache_size;

/* Initialize the node cache, set the maximum number of allowed nodes
   in the cache to SIZE_MAX.  */
void ncache_init (int size_max);

/* Lookup the node for the light node LNODE.  If it does not exist
   anymore in the cache, create a new node.  Store the looked up node
   in *NODE.  */
error_t ncache_node_lookup (lnode_t *lnode, node_t **node);

void ncache_reset (void);

/* Add the given node NODE to the node cache; remove
   least-recently-used nodes, if needed.  */
void ncache_node_add (node_t *node);

#endif
