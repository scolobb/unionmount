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

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <error.h>
#include <stdlib.h>
#include <assert.h>

#include "ncache.h"
#include "lib.h"
#include "unionfs.h"

/* The node cache.  */
ncache_t ncache;

/* Cache size, may be overwritten by the user.  */
int ncache_size = NCACHE_SIZE;

/* Initialize the node cache, set the maximum number of allowed nodes
   in the cache to SIZE_MAX.  */
void
ncache_init (int size_max)
{
  ncache.mru = NULL;
  ncache.lru = NULL;
  ncache.size_max = size_max;
  ncache.size_current = 0;
  mutex_init (&ncache.lock);
}

/* Remove the given node NODE from the cache.  */
static void
ncache_node_remove (node_t *node)
{
  struct netnode *nn = node->nn;

  if (nn->ncache_next)
    nn->ncache_next->nn->ncache_prev = nn->ncache_prev;
  if (nn->ncache_prev)
    nn->ncache_prev->nn->ncache_next = nn->ncache_next;
  if (ncache.mru == node)
    ncache.mru = nn->ncache_next;
  if (ncache.lru == node)
    ncache.lru = nn->ncache_prev;
  nn->ncache_next = NULL;
  nn->ncache_prev = NULL;
  ncache.size_current--;
}

void
ncache_reset (void)
{
  node_t *node;

  mutex_lock (&ncache.lock);
  while ((node = ncache.mru))
    ncache_node_remove (node);
  mutex_unlock (&ncache.lock);
}

/* Lookup the node for the light node LNODE.  If it does not exist
   anymore in the cache, create a new node.  Store the looked up node
   in *NODE.  */
error_t
ncache_node_lookup (lnode_t *lnode, node_t **node)
{
  error_t err = 0;
  node_t *n;

  if (lnode->node)
    {
      debug_msg ("ncache_node_lookup for lnode: %s (found in cache)",
		 lnode->name);
      n = lnode->node;
      netfs_nref (n);
    }
  else
    {
      debug_msg ("ncache_node_lookup for lnode: %s (newly created)",
		 lnode->name);
      err = node_create (lnode, &n);
    }

  if (! err)
    {
      mutex_lock (&n->lock);
      *node = n;
    }
  return err;
}

/* Add the given node NODE to the node cache; remove
   least-recently-used nodes, if needed.  */
void
ncache_node_add (node_t *node)
{
  mutex_lock (&ncache.lock);

  debug_msg ("adding node to cache: %s", node->nn->lnode->name);

  if (ncache.size_max > 0 || ncache.size_current > 0)
    {
      if (ncache.mru != node)
	{
	  if (node->nn->ncache_next || node->nn->ncache_prev)
	    /* Node is already in the cache.  */
	    ncache_node_remove (node);
	  else
	    /* Add a reference from the cache.  */
	    netfs_nref (node);
	  
	  node->nn->ncache_next = ncache.mru;
	  node->nn->ncache_prev = NULL;
	  if (ncache.mru)
	    ncache.mru->nn->ncache_prev = node;
	  if (! ncache.lru)
	    ncache.lru = node;
	  ncache.mru = node;
	  ncache.size_current++;
	}
    }

  /* Forget the least used nodes.  */
  while (ncache.size_current > ncache.size_max)
    {
      struct node *lru = ncache.lru;
      debug_msg ("removing cached node: %s", lru->nn->lnode->name);
      ncache_node_remove (lru);
      netfs_nrele (lru);
    }

  mutex_unlock (&ncache.lock);
}
