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

/* `light node' management.  */

#ifndef INCLUDED_LNODE_H
#define INCLUDED_LNODE_H

#include <hurd/netfs.h>
#include <error.h>

struct lnode
{
  char *name;			/* The name of this light node.  */
  int name_len;			/* This is used quite often and since
				   NAME does not change, just
				   calculate it once.  */
  int flags;			/* Associated flags.  */
  int references;		/* References to this light node.  */
  struct node *node;	        /* Reference to the real node.  */
  struct lnode *next, **prevp;	/* Light nodes are connected in a
				   linked list.  */
  struct lnode *dir;		/* The light node this light node is
				   contained int.  */
  struct lnode *entries;	/* A reference to the list containing
				   the entries of this light node.  */
  struct mutex lock;		/* A lock.  */
};
typedef struct lnode lnode_t;

/* Create a new light node as an entry with the name NAME and store it
   in *NODE.  The new node is not locked and contains a single
   reference.  */
error_t lnode_create (char *name, lnode_t **node);

/* Destroy a light node.  */
void lnode_destroy (lnode_t *node);

/* Install the node in the node tree; add a reference to DIR.  */
void lnode_install (lnode_t *dir, lnode_t *node);

/* Uninstall the node from the node tree; remove a reference from the
   lnode containing NODE.  */
void lnode_uninstall (lnode_t *node);

/* Add a reference to NODE, which must be locked.  */
void lnode_ref_add (lnode_t *node);

/* Remove a reference to NODE, which must be locked.  If that was the
   last reference, destroy the node, otherwise simply unlock NODE.  */
void lnode_ref_remove (lnode_t *node);

/* Get a light node by it's name.  The looked up node is locked and
   got one reference added.  */
error_t lnode_get (lnode_t *dir, char *name,
		   lnode_t **node);

/* Construct the full path for a given light node.  */
error_t lnode_path_construct (lnode_t *node, char **path);

#endif
