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

/* Underlying filesystem management.  */

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>

#include "ulfs.h"

/* The start of the ulfs chain.  */
ulfs_t *ulfs_chain_start;

/* The end of the ulfs chain, we need this, to go through the chain in
   reversed order.  */
ulfs_t *ulfs_chain_end;

/* Number of registered underlying filesystems.  */
unsigned int ulfs_num;

/* The lock protecting the ulfs data structures.  */
struct mutex ulfs_lock = MUTEX_INITIALIZER;

/* Create a new ulfs element.  */
error_t
ulfs_create (char *path, ulfs_t **ulfs)
{
  ulfs_t *ulfs_new = malloc (sizeof (ulfs_t));
  error_t err = 0;
  
  if (! ulfs_new)
    err = ENOMEM;
  else
    {
      char *path_cp = path ? strdup (path) : NULL;

      if (path && (! path_cp))
	{
	  err = ENOMEM;
	  free (ulfs_new);
	}
      else
	{
	  ulfs_new->path = path_cp;
	  ulfs_new->flags = 0;
	  ulfs_new->next = NULL;
	  ulfs_new->prev = NULL;
	  ulfs_new->prevp = NULL;
	  *ulfs = ulfs_new;
	}
    }
  return err;
}

/* Destroy an ulfs element.  */
void
ulfs_destroy (ulfs_t *ulfs)
{
  free (ulfs->path);
  free (ulfs);
}

/* Install ULFS into the linked list of registered filesystems.  */
void
ulfs_install (ulfs_t *ulfs)
{
  ulfs->next = ulfs_chain_start;
  ulfs->prev = NULL;
  ulfs->prevp = &ulfs_chain_start;
  if (ulfs_chain_start)
    {
      ulfs_chain_start->prev = ulfs;
      ulfs_chain_start->prevp = &ulfs->next;
    }
  else
    ulfs_chain_end = ulfs;

  ulfs_chain_start = ulfs;
}

/* Remove ULFS from the linked list of registered filesystems.  */
void
ulfs_uninstall (ulfs_t *ulfs)
{
  *ulfs->prevp = ulfs->next;
  if (ulfs->next)
    {
      ulfs->next->prev = ulfs->prev;
      ulfs->next->prevp = &ulfs->next;
    }
  else
    ulfs_chain_end = ulfs->prev;
}

/* Get an ulfs element by it's index.  */
error_t
ulfs_get_num (int num, ulfs_t **ulfs)
{
  error_t err = EINVAL;
  ulfs_t *u;
  int i;

  for (u = ulfs_chain_start, i = 0;
       u && i < num;
       u = u->next, i++);
  if (u)
    {
      err = 0;
      *ulfs = u;
    }
  return err;
}

/* Get an ulfs element by the associated path.  */
error_t
ulfs_get_path (char *path, ulfs_t **ulfs)
{
  error_t err = ENOENT;
  ulfs_t *u;

  for (u = ulfs_chain_start;
       u && (! (((! path) && path == u->path)
		|| (path && u->path && (! strcmp (path, u->path)))));
       u = u->next);
  if (u)
    {
      err = 0;
      *ulfs = u;
    }
  return err;
}

/* Register a new underlying filesystem.  */
error_t
ulfs_register (char *path, int flags)
{
  ulfs_t *ulfs;
  error_t err;
  
  mutex_lock (&ulfs_lock);
  err = ulfs_create (path, &ulfs);
  if (! err)
    {
      ulfs->flags = flags;
      ulfs_install (ulfs);
      ulfs_num++;
    }
  mutex_unlock (&ulfs_lock);
  return err;
}

/* Unregister an underlying filesystem.  */
error_t
ulfs_unregister (char *path)
{
  ulfs_t *ulfs;
  error_t err;

  mutex_lock (&ulfs_lock);
  err = ulfs_get_path (path, &ulfs);
  if (! err)
    {
      ulfs_uninstall (ulfs);
      ulfs_destroy (ulfs);
      ulfs_num--;
    }
  mutex_unlock (&ulfs_lock);
  return err;
}
