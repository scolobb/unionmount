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

/* node management.  */

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>

#include "unionfs.h"
#include "node.h"
#include "ulfs.h"
#include "lib.h"

/* Declarations for functions only used in this file.  */

/* Deallocate all ports contained in NODE and free per-ulfs data
   structures.  */
void node_ulfs_free (node_t *node);

/* Create a new node, derived from a light node, add a reference to
   the light node.  */
error_t
node_create (lnode_t *lnode, node_t **node)
{
  netnode_t *netnode_new = malloc (sizeof (netnode_t));
  error_t err = 0;
  node_t *node_new;

  debug_msg ("node_create for lnode: %s", lnode->name);

  if (! netnode_new)
    {
      err = ENOMEM;
      return err;
    }

  node_new = netfs_make_node (netnode_new);
  if (! node_new)
    {
      err = ENOMEM;
      free (netnode_new);
      return err;
    }

  node_new->nn->ulfs = NULL;

  err = node_ulfs_init (node_new);
  if (err)
    {
      node_destroy (node_new);
      return err;
    }

  lnode->node = node_new;
  lnode_ref_add (lnode);
  node_new->nn->lnode = lnode;
  node_new->nn->flags = 0;
  node_new->nn->ncache_next = NULL;
  node_new->nn->ncache_prev = NULL;
  *node = node_new;

  return err;
}

/* Destroy a node, remove one reference from the associated light
   node.  */
void
node_destroy (node_t *node)
{
  debug_msg ("node destroy: %s", node->nn->lnode->name);
  assert (! (node->nn->ncache_next || node->nn->ncache_prev));
  node_ulfs_free (node);
  mutex_lock (&node->nn->lnode->lock);
  node->nn->lnode->node = NULL;
  lnode_ref_remove (node->nn->lnode);
  free (node->nn);
  free (node);
}

/* Make sure that all ports to the underlying filesystems of NODE,
   which must be locked, are uptodate.  */
error_t
node_update (node_t *node)
{
  error_t err = 0;
  char *path;

  node_ulfs_t *root_ulfs;
  struct stat stat;
  file_t port;
  int i = 0;
  
  debug_msg ("node_update for lnode: %s", node->nn->lnode->name);

  if (node_is_root (node))
    return err;

  mutex_lock (&netfs_root_node->lock);

  err = lnode_path_construct (node->nn->lnode, &path);
  if (err)
    {
      mutex_unlock (&netfs_root_node->lock);
      return err;
    }

  root_ulfs = netfs_root_node->nn->ulfs;

  node_ulfs_iterate_unlocked (node)
    {
  
      if (node_ulfs->flags & FLAG_NODE_ULFS_FIXED)
	{
	  i++;
	  continue;
	}
      
      /* We really have to update the port.  */
      if (port_valid (node_ulfs->port))
	port_dealloc (node_ulfs->port);

      err = file_lookup ((root_ulfs + i)->port, path,
			 O_READ | O_NOTRANS, O_NOTRANS,
			 0, &port, &stat);
      
      if (err)
	{
	  node_ulfs->port = MACH_PORT_NULL;
	  err = 0;
	  i++;
	  continue;
	}
      
      if (stat.st_ino == underlying_node_stat.st_ino
	  && stat.st_fsid == underlying_node_stat.st_fsid)
	/* It's OUR root node.  */
	err = ELOOP;
      else
	{
	  port_dealloc (port);
	  err = file_lookup ((root_ulfs + i)->port, path,
			     O_READ, 0, 0, &port, &stat);
	}
      
      if (err)
	{
	  port = MACH_PORT_NULL;
	  err = 0;
	}
      node_ulfs->port = port;
      
      i++;
    }

  free (path);
  node->nn->flags |= FLAG_NODE_ULFS_UPTODATE;

  mutex_unlock (&netfs_root_node->lock);
  
  return err;
}

/* Remove all directory named NAME beneath DIR on all underlying filesystems.
   Fails if we cannot remove all the directories.  */
error_t
node_dir_remove (node_t *dir, char *name)
{
  error_t err = 0;

  node_ulfs_iterate_reverse_unlocked (dir)
    {
      if (!port_valid (node_ulfs->port))
	continue;

      err = dir_rmdir (node_ulfs->port, name);
      if ((err) && (err != ENOENT))
	break;
    }

  return err;
}

/* Create a directory named NAME beneath DIR on the first (writable) underlying
   filesystem.  */
error_t
node_dir_create (node_t *dir, char *name, mode_t mode)
{
  error_t err = 0;

  node_ulfs_iterate_unlocked (dir)
    {
      if (!port_valid (node_ulfs->port))
	continue;
      
      err = dir_mkdir (node_ulfs->port, name, mode);

      if ((!err) || (err == EEXIST) || (err == ENOTDIR))
	break;
    }
  
  return err;
}

/* Remove all files named NAME beneath DIR on the underlying filesystems
   with FLAGS as openflags.  */
error_t
node_unlink_file (node_t *dir, char *name)
{
  error_t err = 0;
  int removed = 0;

  /* Using reverse iteration still have issues. Infact, we could be
     deleting a file in some underlying filesystem, and keeping those
     after the first occurring error. 
     FIXME: Check BEFORE starting deletion.  */
     
  node_ulfs_iterate_reverse_unlocked (dir)
    {
      
      if (!port_valid (node_ulfs->port))
	continue;
      
      err = dir_unlink (node_ulfs->port, name);
      if ((err) && (err != ENOENT))
	break;

      if (!err)
	removed++;

      /* Ignore ENOENT.  */
      err = 0;
    }

  if (!removed)
    err = ENOENT;

  return err;
}

/* Lookup a file named NAME beneath DIR on the underlying filesystems
   with FLAGS as openflags.  Return the first port successfully looked
   up in *PORT and according stat information in *STAT.  */
error_t
node_lookup_file (node_t *dir, char *name, int flags,
		  file_t *port, struct stat *s)
{
  error_t err = ENOENT;
  struct stat stat;
  file_t p;

  node_ulfs_iterate_unlocked (dir)
    {

      if (err != ENOENT)
	break;

      if (!port_valid (node_ulfs->port))
	continue;

      err = file_lookup (node_ulfs->port, name,
			 flags | O_NOTRANS, O_NOTRANS,
			 0, &p, &stat);
      if (err)
	continue;

      if (stat.st_ino == underlying_node_stat.st_ino
	  && stat.st_fsid == underlying_node_stat.st_fsid)
	/* It's OUR root node.  */
	err = ELOOP;
      else 
	/* stat.st_mode & S_ITRANS  */
	{
	  port_dealloc (p);
	  err = file_lookup (node_ulfs->port, name,
			     flags, 0, 0, &p, &stat);
	}
    }

  if (! err)
    {
      *s = stat;
      *port = p;
    }

  return err;
}

/* Deallocate all ports contained in NODE and free per-ulfs data
   structures.  */
void
node_ulfs_free (node_t *node)
{

  node_ulfs_iterate_unlocked (node)
    {
      if (port_valid (node_ulfs->port)
	  && node_ulfs->port != underlying_node)
	port_dealloc (node_ulfs->port);
    }

  free (node->nn->ulfs);
}

/* Initialize per-ulfs data structures for NODE.  The ulfs_lock must
   be held by the caller.  */
error_t
node_ulfs_init (node_t *node)
{
  node_ulfs_t *ulfs_new;
  error_t err = 0;
  
  ulfs_new = malloc (ulfs_num * sizeof (node_ulfs_t));
  if (! ulfs_new)
    {
      err = ENOMEM;
      return err;
    }

  if (node->nn->ulfs)
    node_ulfs_free (node);

  node->nn->ulfs = ulfs_new;
  node->nn->ulfs_num = ulfs_num;
      
  node_ulfs_iterate_unlocked (node)
    {
      node_ulfs->flags = 0;
      node_ulfs->port = port_null;
    }

  return err;
}

/* Read the merged directory entries from NODE, which must be
   locked, into *DIRENTS.  */
error_t
node_entries_get (node_t *node, node_dirent_t **dirents)
{
  struct dirent **dirent_list, **dirent;
  node_dirent_t *node_dirent_list = NULL;
  size_t dirent_data_size;
  char *dirent_data;
  error_t err = 0;

  /* Add a dirent to the list.  If an entry with the specified name
     already exists, reuse that entry.  Otherwise create a new
     one.  */
  error_t node_dirent_add (char *name, ino_t fileno, int type)
    {
      error_t e = 0;
      node_dirent_t *node_dirent;
      node_dirent_t *node_dirent_new;
      struct dirent *dirent_new;
      int name_len = strlen (name);
      int size = DIRENT_LEN (name_len);


      for (node_dirent = node_dirent_list;
	   node_dirent && strcmp (node_dirent->dirent->d_name, name);
	   node_dirent = node_dirent->next);

      if (node_dirent)
	{
	  /* Reuse existing entry.  */

	  node_dirent->dirent->d_fileno = fileno;
	  node_dirent->dirent->d_type = type;
	  return e;
	}

      /* Create new entry.  */
      
      node_dirent_new = malloc (sizeof (node_dirent_t));
      if (!node_dirent_new)
	{
	  e = ENOMEM;
	  return e;
	}

      dirent_new = malloc (size);
      if (!dirent_new)
	{
	  free (node_dirent_new);
	  e = ENOMEM;
	  return e;
	}

      /* Fill dirent.  */
      dirent_new->d_fileno = fileno;
      dirent_new->d_type = type;
      dirent_new->d_reclen = size;
      strcpy ((char *) dirent_new + DIRENT_NAME_OFFS, name);
      
      /* Add dirent to the list.  */
      node_dirent_new->dirent = dirent_new;
      node_dirent_new->next = node_dirent_list;
      node_dirent_list = node_dirent_new;
      
      return e;
    }

  node_ulfs_iterate_unlocked(node)
    {
      if (!port_valid (node_ulfs->port))
	continue;

      err = dir_entries_get (node_ulfs->port, &dirent_data,
			     &dirent_data_size, &dirent_list);
      if (err)
	continue;
      
      for (dirent = dirent_list; (! err) && *dirent; dirent++)
	if (strcmp ((*dirent)->d_name, ".")
	    && strcmp ((*dirent)->d_name, ".."))
	  err = node_dirent_add ((*dirent)->d_name,
				 (*dirent)->d_fileno,
				 (*dirent)->d_type);

      free (dirent_list);
      munmap (dirent_data, dirent_data_size);
    }

  if (err)
    node_entries_free (node_dirent_list);
  else
    *dirents = node_dirent_list;

  return err;
}

/* Free DIRENTS.  */
void
node_entries_free (node_dirent_t *dirents)
{
  node_dirent_t *dirent, *dirent_next;

  for (dirent = dirents; dirent; dirent = dirent_next)
    {
      dirent_next = dirent->next;
      free (dirent->dirent);
      free (dirent);
    }
}

/* Create the root node (and it's according lnode) and store it in
   *ROOT_NODE.  */
error_t
node_create_root (node_t **root_node)
{
  lnode_t *lnode;
  node_t *node;
  error_t err = 0;

  err = lnode_create (NULL, &lnode);
  if (err)
    return err;

  err = node_create (lnode, &node);
  if (err)
    {
      lnode_destroy (lnode);
      return err;
    }

  mutex_unlock (&lnode->lock);
  *root_node = node;
  return err;
}

/* Initialize the ports to the underlying filesystems for the root
   node.  */

error_t
node_init_root (node_t *node)
{
  error_t err;
  ulfs_t *ulfs;
  int i = 0;

  mutex_lock (&ulfs_lock);

  err = node_ulfs_init (node);
  if (err)
    {
      mutex_unlock (&ulfs_lock);
      return err;
    }

  node_ulfs_iterate_unlocked (node)
    {

      if (err)
	break;

      err = ulfs_get_num (i, &ulfs);
      if (err)
	  break;

      if (ulfs->path)
	node_ulfs->port = file_name_lookup (ulfs->path,
					    O_READ | O_DIRECTORY, 0);
      else
	node_ulfs->port = underlying_node;
	  
      if (! port_valid (node_ulfs->port))
	{
	  err = errno;
	  break;
	}

      node_ulfs->flags |= FLAG_NODE_ULFS_FIXED;
      i++;
    }

  mutex_unlock (&ulfs_lock);
  return err;
}
