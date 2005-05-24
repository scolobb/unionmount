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

/* node management.  */

#ifndef INCLUDED_NODE_H
#define INCLUDED_NODE_H

#include <error.h>
#include <sys/stat.h>
#include <hurd/netfs.h>

typedef struct node node_t;

#include "lnode.h"

/* per-ulfs data for each node.  */
struct node_ulfs
{
  int flags;			/* Flags associated for this
				   underlying filesystem.  */
  file_t port;			/* A port to the underlying
				   filesystem.  */
};
typedef struct node_ulfs node_ulfs_t;

/* Flags.  */

/* The according port should not be updated.  */
#define FLAG_NODE_ULFS_FIXED 0x00000001

struct netnode
{
  lnode_t *lnode;		/* A reference to the according light
				   node.  */
  int flags;			/* Associated flags.  */
  node_ulfs_t *ulfs;		/* Array holding data for each
				   underlying filesystem.  */
  int ulfs_num;			/* Number of entries in ULFS.  */
  node_t *ncache_next;
  node_t *ncache_prev;
};
typedef struct netnode netnode_t;

/* Flags.  */
#define FLAG_NODE_INVALIDATE    0x00000001
#define FLAG_NODE_ULFS_UPTODATE 0x00000002

typedef struct node_dirent
{
  struct dirent *dirent;
  struct node_dirent *next;
} node_dirent_t;

/* Create a new node, derived from a light node, add a reference to
   the light node.  */
error_t node_create (lnode_t *lnode, node_t **node);

/* Destroy a node, remove one reference from the associated light
   node.  */
void node_destroy (node_t *node);

/* Make sure that all ports to the underlying filesystems of NODE,
   which must be locked, are uptodate.  */
error_t node_update (node_t *node);

/* Remove all files named NAME beneath DIR on the underlying filesystems
   with FLAGS as openflags.  */
error_t node_unlink_file (node_t *dir, char *name);

/* Lookup a file named NAME beneath DIR on the underlying filesystems
   with FLAGS as openflags.  Return the first port successfully looked
   up in *PORT and according stat information in *STAT.  */
error_t node_lookup_file (node_t *dir, char *name, int flags,
			  file_t *port, struct stat *stat);

/* Initialize per-ulfs data structures for NODE.  The ulfs_lock must
   be held by the caller.  */
error_t node_ulfs_init (node_t *node);

/* Read the merged directory entries from NODE, which must be
   locked, into *DIRENTS.  */
error_t node_entries_get (node_t *node, node_dirent_t **dirents);

/* Free DIRENTS.  */
void node_entries_free (node_dirent_t *dirents);

/* Create the root node (and it's according lnode) and store it in
   *ROOT_NODE.  */
error_t node_create_root (node_t **root_node);

/* Initialize the ports to the underlying filesystems for the root
   node.  */
error_t node_init_root (node_t *node);

/* Return non-zero, if NODE is the root node.  */
#define node_is_root(node) (node)->nn->lnode->dir ? 0 : 1

/* Iterate over the per-ulfs data in NODE, which must be locked by the
   caller.  */
#define node_ulfs_iterate_unlocked(node)              \
  for (node_ulfs_t *node_ulfs = (node)->nn->ulfs;     \
       node_ulfs < (node)->nn->ulfs + (node)->nn->ulfs_num;  \
       node_ulfs++)

#define node_ulfs_iterate_reverse_unlocked(node)			\
  for (node_ulfs_t *node_ulfs = (node)->nn->ulfs + (node)->nn->ulfs_num - 1;\
       node_ulfs >= (node)->nn->ulfs;					\
       node_ulfs--)

#endif
