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

/* `light node' management.  See unionfs.h for an explanation of light
   nodes.  */

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>

#include "lnode.h"
#include "lib.h"
#include "unionfs.h"

/* Create a new light node as an entry with the name NAME and store it
   in *NODE.  The new node is not locked and contains a single
   reference.  */
error_t
lnode_create (char *name, lnode_t **node)
{
  lnode_t *node_new = malloc (sizeof (lnode_t));
  error_t err = 0;
  
  debug_msg ("lnode_create for name: %s", name);

  if (! node_new)
    err = ENOMEM;
  else
    {
      char *name_cp = NULL;

      if (name)
	name_cp = strdup (name);
      if (name && (! name_cp))
	{
	  err = ENOMEM;
	  free (node_new);
	}
      else
	{
	  node_new->name = name_cp;
	  node_new->name_len = name_cp ? strlen (name_cp) : 0;
	  node_new->flags = 0;
	  node_new->node = NULL;
	  node_new->next = NULL;
	  node_new->prevp = NULL;
	  node_new->dir = NULL;
	  node_new->entries = NULL;
	  node_new->references = 1;
	  mutex_init (&node_new->lock);
	  mutex_lock (&node_new->lock);
	  *node = node_new;
	}
    }
  return err;
}

/* Destroy a light node.  */
void
lnode_destroy (lnode_t *node)
{
  debug_msg ("lnode_destroy for name: %s", node->name);
  free (node->name);
  free (node);
}

/* Install the node in the node tree; add a reference to DIR, which
   must be locked.  */
void
lnode_install (lnode_t *dir, lnode_t *node)
{
  lnode_ref_add (dir);
  node->next = dir->entries;
  node->prevp = &dir->entries;
  if (dir->entries)
    dir->entries->prevp = &node->next;
  dir->entries = node;
  node->dir = dir;
}

/* Uninstall the node from the node tree; remove a reference from the
   lnode containing NODE.  */
void
lnode_uninstall (lnode_t *node)
{
  lnode_ref_remove (node->dir);
  *node->prevp = node->next;
  if (node->next)
    node->next->prevp = &node->next;
}

/* Add a reference to NODE, which must be locked.  */
void
lnode_ref_add (lnode_t *node)
{
  node->references++;
}

/* Remove a reference to NODE, which must be locked.  If that was the
   last reference, destroy the node, otherwise simply unlock NODE.  */
void
lnode_ref_remove (lnode_t *node)
{
  assert (node->references);
  if (! --node->references)
    {
      lnode_uninstall (node);
      lnode_destroy (node);
    }
  else
    mutex_unlock (&node->lock);
}

/* Get a light node by it's name.  The looked up node is locked and
   got one reference added.  */
error_t
lnode_get (lnode_t *dir, char *name,
	   lnode_t **node)
{
  error_t err = 0;
  lnode_t *n;

  for (n = dir->entries; n && strcmp (n->name, name); n = n->next);
  if (n)
    {
      mutex_lock (&n->lock);
      lnode_ref_add (n);
      *node = n;
    }
  else
    err = ENOENT;

  return err;
}

/* Construct the full path for a given light node.  */
error_t
lnode_path_construct (lnode_t *node,
		      char **path)
{
  error_t err = 0;
  int p_len = 1;
  lnode_t *n;
  char *p;

  for (n = node; n && n->dir; n = n->dir)
    p_len += n->name_len + (n->dir->dir ? 1 : 0);

  p = malloc (p_len);
  if (! p)
    err = ENOMEM;
  else
    {
      *(p + --p_len) = 0;
      for (n = node; n && n->dir; n = n->dir)
	{
	  p_len -= n->name_len;
	  strncpy (p + p_len, n->name, n->name_len);
	  if (n->dir->dir)
	    *(p + --p_len) = '/';
	}
      *path = p;
    }
  return err;
}
