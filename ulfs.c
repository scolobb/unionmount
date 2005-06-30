/* Hurd unionfs
   Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.
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

#include "unionfs.h"
#include <fcntl.h>

#include "lib.h"
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

/* Install ULFS into the linked list of registered filesystems in
 * priority order.  */
void
ulfs_install (ulfs_t *ulfs)
{
  ulfs_t *u = ulfs_chain_start;
  int insert_at_end = 0;
  if (ulfs_num == 0)
    {
      ulfs_chain_start = ulfs;
      return;
    }

  /* walk the chain until a filesystem has a priority that's too high. */
  while (u->priority > ulfs->priority)
    {
      if (u->next == NULL)
	{
	  insert_at_end = 1;
	  break;
	}
      u = u->next;
    }

  if (insert_at_end)
    {
      u->next = ulfs;
      ulfs->prev = u;
    }
  else
    {
      if (u == ulfs_chain_start)
	{
	  ulfs_chain_start = ulfs;
	  ulfs->next = u;
	  ulfs->prev = NULL;
	  u->prev = ulfs;
	}
      else
	{
	  ulfs->next = u;
	  ulfs->prev = u->prev;
	  u->prev->next = ulfs;
	  u->prev = ulfs;
	}
    }

  return;
}

/* Remove ULFS from the linked list of registered filesystems.  */
void
ulfs_uninstall (ulfs_t *ulfs)
{
  if (ulfs == ulfs_chain_start)
      ulfs_chain_start = ulfs->next;

  if (ulfs->next)
    ulfs->next->prev = ulfs->prev;

  if (ulfs->prev)
    ulfs->prev->next = ulfs->next;
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

error_t
ulfs_for_each_under_priv (char *path_under,
			  error_t (*func) (char *, char *, void *),
			  void *priv)
{
  error_t err = 0;
  ulfs_t *u;
  size_t length;
  
  length = strlen (path_under);

  for (u = ulfs_chain_start; u; u = u->next)
    {
      if (!u->path)
	continue;

      if (memcmp (u->path, path_under, length))
	continue;

      /* This ulfs is under path_under.  */
      func ((char *)(u->path + length), path_under, priv);
    }

  return err;
}

/* Register a new underlying filesystem.  */
error_t
ulfs_register (char *path, int flags, int priority)
{
  ulfs_t *ulfs;
  error_t err;

  if (path)
    {
      err = check_dir (path);
      if (err)
	  return err;
    }

  mutex_lock (&ulfs_lock);
  err = ulfs_create (path, &ulfs);
  if (! err)
    {
      ulfs->flags = flags;
      ulfs->priority = priority;
      ulfs_install (ulfs);
      ulfs_num++;
    }
  mutex_unlock (&ulfs_lock);
  return err;
}

/* Check for deleted ulfs entries.  */
/* FIXME: Ugly as hell. Rewrite the whole ulfs.c  */
void
ulfs_check ()
{
  ulfs_t *u;
  file_t p;

  struct ulfs_destroy
  {
    ulfs_t *ulfs;

    struct ulfs_destroy *next;
  } *ulfs_destroy_q = NULL;

  mutex_lock (&ulfs_lock);

  u = ulfs_chain_start;
  while (u)
    {
      
      if (u->path)
	p = file_name_lookup (u->path, O_READ | O_DIRECTORY, 0);
      else
	p = underlying_node;
	  
      if (! port_valid (p))
	{
	  struct ulfs_destroy *ptr;

	  /* Add to destroy list.  */
	  ptr = malloc (sizeof (struct ulfs_destroy));
	  assert (ptr);

	  ptr->ulfs = u;

	  ptr->next = ulfs_destroy_q;
	  ulfs_destroy_q = ptr;
	}
	  
      u = u->next;
    }

  while (ulfs_destroy_q)
    {
      struct ulfs_destroy *ptr;

      ptr = ulfs_destroy_q;
      ulfs_destroy_q = ptr->next;

      ulfs_uninstall (ptr->ulfs);
      ulfs_destroy (ptr->ulfs);
      ulfs_num--;	  

      free (ptr);
    }

  mutex_unlock (&ulfs_lock);

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
