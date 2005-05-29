/* Hurd unionfs
   Copyright (C) 2001, 2002, 2003, 2005 Free Software Foundation, Inc.
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
#include <argz.h>
#include <stddef.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <hurd/paths.h>
#include <sys/mman.h>

#include "unionfs.h"
#include "ulfs.h"
#include "node.h"
#include "lib.h"
#include "ncache.h"
#include "options.h"

/* Return an argz string describing the current options.  Fill *ARGZ
   with a pointer to newly malloced storage holding the list and *LEN
   to the length of that storage.  */
error_t
netfs_append_args (char **argz, size_t *argz_len)
{
  error_t err = 0;

  ulfs_iterate
    {
      if (! err)
	if (unionfs_flags & FLAG_UNIONFS_MODE_DEBUG)
	  err = argz_add (argz, argz_len,
			  OPT_LONG (OPT_LONG_DEBUG));
      if (! err)
	if (ulfs->flags & FLAG_ULFS_WRITABLE)
	  err = argz_add (argz, argz_len,
			  OPT_LONG (OPT_LONG_WRITABLE));
      if (! err)
	{
	  if (ulfs->path)
	    err = argz_add (argz, argz_len, ulfs->path);
	  else
	    err = argz_add (argz, argz_len,
			    OPT_LONG (OPT_LONG_UNDERLYING));
	}
    }

  return err;
}

#ifndef __USE_FILE_OFFSET64
#define OFFSET_T __off_t		/* Size in bytes.  */
#else
#define OFFSET_T __off64_t		/* Size in bytes.  */
#endif

static error_t
_get_node_size (struct node *dir, OFFSET_T *off)
{
  size_t size = 0;
  error_t err;
  int count = 0;
  node_dirent_t *dirent_start, *dirent_current;
  node_dirent_t *dirent_list = NULL;
  int first_entry = 2;
  
  int bump_size (const char *name)
    {
      size_t new_size = size + DIRENT_LEN (strlen (name));
      
      size = new_size;
      count ++;
      return 1;
    }
  
  err = node_entries_get (dir, &dirent_list);
  if (err)
    return err;
  
  for (dirent_start = dirent_list, count = 2;
       dirent_start && first_entry > count;
       dirent_start = dirent_start->next, count++);
  
  count = 0;
  
  /* Make space for the `.' and `..' entries.  */
  if (first_entry == 0)
    bump_size (".");
  if (first_entry <= 1)
    bump_size ("..");
  
  /* See how much space we need for the result.  */
  for (dirent_current = dirent_start;
       dirent_current;
       dirent_current = dirent_current->next)
    if (! bump_size (dirent_current->dirent->d_name))
      break;

  free (dirent_list);

  *off = size;

  return 0;
}


/* Make sure that NP->nn_stat is filled with current information.
   CRED identifies the user responsible for the operation. */
error_t
netfs_validate_stat (struct node *np, struct iouser *cred)
{
  error_t err = 0;

  if (np != netfs_root_node)
    {
      if (! (np->nn->flags & FLAG_NODE_ULFS_UPTODATE))
	err = node_update (np);
      if (! err)
	{
	  int done = 0;

	  node_ulfs_iterate_unlocked (np)
	    if ((! done) && port_valid (node_ulfs->port))
	      {
		err = io_stat (node_ulfs->port, &np->nn_stat);
		if (! err)
		  np->nn_translated = np->nn_stat.st_mode;
		done = 1;
	      }
	  if (! done)
	    err = ENOENT;	/* FIXME?  */
	}
    }
  else 
    {
      _get_node_size (np, &np->nn_stat.st_size); 
    }

  return err;
}

/* This should attempt a chmod call for the user specified by CRED on
   locked node NP, to change the owner to UID and the group to GID.  */
error_t
netfs_attempt_chown (struct iouser *cred, struct node *np,
		     uid_t uid, uid_t gid)
{
  return EOPNOTSUPP;
}

/* This should attempt a chauthor call for the user specified by CRED
   on locked node NP, thereby changing the author to AUTHOR.  */
error_t
netfs_attempt_chauthor (struct iouser *cred, struct node *np,
			uid_t author)
{
  return EOPNOTSUPP;
}

/* This should attempt a chmod call for the user specified by CRED on
   locked node NODE, to change the mode to MODE.  Unlike the normal
   Unix and Hurd meaning of chmod, this function is also used to
   attempt to change files into other types.  If such a transition is
   attempted which is impossible, then return EOPNOTSUPP.  */
error_t
netfs_attempt_chmod (struct iouser *cred, struct node *np,
		     mode_t mode)
{
  return EOPNOTSUPP;
}

/* Attempt to turn locked node NP (user CRED) into a symlink with
   target NAME.  */
error_t
netfs_attempt_mksymlink (struct iouser *cred, struct node *np,
			 char *name)
{
  return EOPNOTSUPP;
}

/* Attempt to turn NODE (user CRED) into a device.  TYPE is either
   S_IFBLK or S_IFCHR.  NP is locked.  */
error_t
netfs_attempt_mkdev (struct iouser *cred, struct node *np,
		     mode_t type, dev_t indexes)
{
  return EOPNOTSUPP;
}

/* Attempt to set the passive translator record for FILE to ARGZ (of
   length ARGZLEN) for user CRED. NP is locked.  */
error_t
netfs_set_translator (struct iouser *cred, struct node *np,
		      char *argz, size_t argzlen)
{
  return EOPNOTSUPP;
}

/* For locked node NODE with S_IPTRANS set in its mode, look up the
   name of its translator.  Store the name into newly malloced
   storage, and return it in *ARGZ; set *ARGZ_LEN to the total length.  */
error_t
netfs_get_translator (struct node *node, char **argz,
		      size_t *argz_len)
{
  return EOPNOTSUPP;
}

/* This should attempt a chflags call for the user specified by CRED
   on locked node NP, to change the flags to FLAGS.  */
error_t
netfs_attempt_chflags (struct iouser *cred, struct node *np,
		       int flags)
{
  return EOPNOTSUPP;
}

/* This should attempt a utimes call for the user specified by CRED on
   locked node NP, to change the atime to ATIME and the mtime to
   MTIME.  If ATIME or MTIME is null, then set to the current time.  */
error_t
netfs_attempt_utimes (struct iouser *cred, struct node *np,
		      struct timespec *atime, struct timespec *mtime)
{
  return 0;
}

/* This should attempt to set the size of the locked file NP (for user
   CRED) to SIZE bytes long.  */
error_t
netfs_attempt_set_size (struct iouser *cred, struct node *np,
			off_t size)
{
  return EOPNOTSUPP;
}

/* This should attempt to fetch filesystem status information for the
   remote filesystem, for the user CRED. NP is locked.  */
error_t
netfs_attempt_statfs (struct iouser *cred, struct node *np,
		      struct statfs *st)
{
  return EOPNOTSUPP;
}

/* This should sync the locked file NP completely to disk, for the
   user CRED.  If WAIT is set, return only after the sync is
   completely finished.  */
error_t
netfs_attempt_sync (struct iouser *cred, struct node *np,
		    int wait)
{
  return EOPNOTSUPP;
}

/* This should sync the entire remote filesystem.  If WAIT is set,
   return only after the sync is completely finished.  */
error_t
netfs_attempt_syncfs (struct iouser *cred, int wait)
{
  return 0;
}

/* lookup */

/* We don't use this functions, but it has to be defined.  */
error_t
netfs_attempt_lookup (struct iouser *user, struct node *dir,
		      char *name, struct node **node)
{
  return EOPNOTSUPP;
}

/* Delete NAME in DIR (which is locked) for USER.  */
error_t
netfs_attempt_unlink (struct iouser *user, struct node *dir,
		      char *name)
{
  error_t err = 0;
  mach_port_t p;
  struct stat statbuf;

  node_update (dir);

  err = node_lookup_file (dir, name, 0, &p, &statbuf);
  if (err)
      return err;

  port_dealloc (p);

  err = fshelp_checkdirmod (&dir->nn_stat, &statbuf, user);
  if (err)
      return err;

  err = node_unlink_file (dir, name);

  return err;
}

/* Attempt to rename the directory FROMDIR to TODIR. Note that neither
   of the specific nodes are locked.  */
error_t
netfs_attempt_rename (struct iouser *user, struct node *fromdir,
		      char *fromname, struct node *todir, 
		      char *toname, int excl)
{
  return EOPNOTSUPP;
}

/* Attempt to create a new directory named NAME in DIR (which is
   locked) for USER with mode MODE. */
error_t
netfs_attempt_mkdir (struct iouser *user, struct node *dir,
		     char *name, mode_t mode)
{
  error_t err = 0;
  mach_port_t p;
  struct stat statbuf;

  node_update (dir);

  err = fshelp_checkdirmod (&dir->nn_stat, 0, user);
  if (err)
    goto exit;

  /* Special case for no UID processes (like login shell).  */
  if ((!user->uids->ids) || (!user->uids->ids))
    {
      err = EACCES;
      goto exit;
    }

  err = node_dir_create (dir, name, mode);
  if (err)
    goto exit;

  err = node_lookup_file (dir, name, 0, &p, &statbuf);
  if (err)
    {
      node_dir_remove (dir, name);
      goto exit;
    }

  err = file_chown (p, user->uids->ids[0], user->gids->ids[0]);
  if (err)
    {
      port_dealloc (p);
      node_dir_remove (dir, name);
      goto exit;
    }

  port_dealloc (p);

 exit:

  return err;
}

/* Attempt to remove directory named NAME in DIR (which is locked) for
   USER.  */
error_t
netfs_attempt_rmdir (struct iouser *user, 
		     struct node *dir, char *name)
{
  error_t err = 0;
  mach_port_t p;
  struct stat statbuf;

  node_update (dir);

  err = node_lookup_file (dir, name, 0, &p, &statbuf);
  if (err)
      return err;

  port_dealloc (p);

  err = fshelp_checkdirmod (&dir->nn_stat, &statbuf, user);
  if (err)
      return err;

  err = node_dir_remove (dir, name);

  return err;
}

/* Create a link in DIR with name NAME to FILE for USER. Note that
   neither DIR nor FILE are locked. If EXCL is set, do not delete the
   target.  Return EEXIST if NAME is already found in DIR.  */
error_t
netfs_attempt_link (struct iouser *user, struct node *dir,
		    struct node *file, char *name, int excl)
{
  return EOPNOTSUPP;
}

/* Attempt to create an anonymous file related to DIR (which is
   locked) for USER with MODE.  Set *NP to the returned file upon
   success. No matter what, unlock DIR.  */
error_t
netfs_attempt_mkfile (struct iouser *user, struct node *dir,
		      mode_t mode, struct node **np)
{
  mutex_unlock (&dir->lock);
  return EOPNOTSUPP;
}

/* (We don't use this function!)  Attempt to create a file named NAME
   in DIR (which is locked) for USER with MODE.  Set *NP to the new
   node upon return.  On any error, clear *NP.  *NP should be locked
   on success; no matter what, unlock DIR before returning.  */
error_t
netfs_attempt_create_file (struct iouser *user, struct node *dir,
			   char *name, mode_t mode, struct node **np)
{
  mutex_unlock (&dir->lock);
  return EOPNOTSUPP;
}

/* We use this local interface to attempt_create file since we are
   using our own netfs_S_dir_lookup.  */
error_t
netfs_attempt_create_file_reduced (struct iouser *user, struct node *dir,
				   char *name, mode_t mode, int flags)
{
  mach_port_t p;
  error_t err;
  struct stat statbuf;

  node_update (dir);

  err = fshelp_checkdirmod (&dir->nn_stat, 0, user);
  if (err)
    goto exit;

  /* Special case for no UID processes (like login shell).  */
  if ((!user->uids->ids) || (!user->uids->ids))
    {
      err = EACCES;
      goto exit;
    }
  
  mutex_unlock (&dir->lock);
  err = node_lookup_file (dir, name, flags | O_CREAT, 
			  &p, &statbuf);
  mutex_lock (&dir->lock);

  if (err)
    goto exit;

  err = file_chmod (p, mode);
  if (err)
    {
      port_dealloc (p);
      node_unlink_file (dir, name);
      goto exit;
    }

  err = file_chown (p, user->uids->ids[0], user->gids->ids[0]);
  if (err)
    {
      port_dealloc (p);
      node_unlink_file (dir, name);
      goto exit;
    }

  err = io_stat (p, &statbuf);

  /* Check file permissions.  */
  if (! err && (flags & O_READ))
    err = fshelp_access (&statbuf, S_IREAD, user);
  if (! err && (flags & O_WRITE))
    err = fshelp_access (&statbuf, S_IWRITE, user);
  if (! err && (flags & O_EXEC))
    err = fshelp_access (&statbuf, S_IEXEC, user);
  
  if (err)
    {
      port_dealloc (p);
      node_unlink_file (dir, name);
      goto exit;
    }

  port_dealloc (p);
  
 exit:
  mutex_unlock (&dir->lock);
  return err;
}

/* Read the contents of locked node NP (a symlink), for USER, into
   BUF.  */
error_t
netfs_attempt_readlink (struct iouser *user, struct node *np,
			char *buf)
{
  return EOPNOTSUPP;
}

/* libnetfs uses this functions once.  */
error_t
netfs_check_open_permissions (struct iouser *user, struct node *np,
			      int flags, int newnode)
{
  error_t err = 0;

  if (! err && (flags & O_READ))
    err = fshelp_access (&np->nn_stat, S_IREAD, user);
  if (! err && (flags & O_WRITE))
    err = fshelp_access (&np->nn_stat, S_IWRITE, user);
  if (! err && (flags & O_EXEC))
    err = fshelp_access (&np->nn_stat, S_IEXEC, user);

  return err;
}

/* Read from the locked file NP for user CRED starting at OFFSET and
   continuing for up to *LEN bytes.  Put the data at DATA.  Set *LEN
   to the amount successfully read upon return.  */
error_t
netfs_attempt_read (struct iouser *cred, struct node *np,
		    off_t offset, size_t *len, void *data)
{
  *len = 0;
  return 0;
}

/* Write to the locked file NP for user CRED starting at OFSET and
   continuing for up to *LEN bytes from DATA.  Set *LEN to the amount
   successfully written upon return.  */
error_t
netfs_attempt_write (struct iouser *cred, struct node *np,
		     off_t offset, size_t *len, void *data)
{
  /* Since unionfs only manages directories...  */
  return EISDIR;
}

/* Return the valid access types (bitwise OR of O_READ, O_WRITE, and
   O_EXEC) in *TYPES for locked file NP and user CRED.  */
error_t
netfs_report_access (struct iouser *cred, struct node *np,
		     int *types)
{
  *types = 0;
  if (fshelp_access (&np->nn_stat, S_IREAD, cred) == 0)
    *types |= O_READ;
  if (fshelp_access (&np->nn_stat, S_IWRITE, cred) == 0)
    *types |= O_WRITE;
  if (fshelp_access (&np->nn_stat, S_IEXEC, cred) == 0)
    *types |= O_EXEC;
  return 0;
}

/* Create a new user from the specified UID and GID arrays.  */
struct iouser *
netfs_make_user (uid_t *uids, int nuids, uid_t *gids, int ngids)
{
  return NULL;
}

/* Node NP has no more references; free all its associated storage.  */
void
netfs_node_norefs (struct node *np)
{
  node_destroy (np);
}

error_t
netfs_attempt_lookup_improved (struct iouser *user, struct node *dir,
			       char *name, struct node **np,
			       int flags, int lastcomp,
			       mach_port_t *port,
			       mach_msg_type_name_t *port_type)
{
  mach_port_t p;
  error_t err;

  mutex_lock (&dir->nn->lnode->lock);

  err = fshelp_access (&dir->nn_stat, S_IEXEC, user);
  if (err)
      goto exit;
  

  if (! *name || ! strcmp (name, "."))
    {

      /* The same node is wanted.  */
      *np = dir;
      netfs_nref (*np);

    }
  else if (! strcmp (name, ".."))
    {

      /* We have to get the according light node first.  */
      lnode_t *lnode = dir->nn->lnode;
      node_t *node;

      err = ncache_node_lookup (lnode->dir, &node);
      if (err)
	goto exit;
	      
      *np = node;
      
    }
  else 
    {

      lnode_t *dir_lnode = dir->nn->lnode;
      struct stat statbuf;
      lnode_t *lnode = NULL;

      /* Lookup the node by it's name on the underlying
	 filesystems.  */

      err = node_update (dir);

      /* We have to unlock this node while doing lookups.  */
      mutex_unlock (&dir_lnode->lock);
      mutex_unlock (&dir->lock);

      err = node_lookup_file (dir, name, flags & ~(O_NOLINK|O_CREAT),
			      &p, &statbuf);

      mutex_lock (&dir->lock);
      mutex_lock (&dir_lnode->lock);


      if (err)
	goto exit;

      if (S_ISDIR (statbuf.st_mode))
	{
	  node_t *node;

	  /* We don't need this port directly.  */
	  port_dealloc (p);
	  
	  /* The found node is a directory, so we have to manage the
	     node.  First we need the light node.  */

	  err = lnode_get (dir_lnode, name, &lnode);
	  if (err == ENOENT)
	    {
	      /* It does not exist, we have to create it.  */
	      err = lnode_create (name, &lnode);
	      if (err)
		goto exit;
	      
	      lnode_install (dir_lnode, lnode);
	    }
	  
	  /* Now we have a light node.  */
	  err = ncache_node_lookup (lnode, &node);
	  
	  /* This unlocks the node for us.  */
	  lnode_ref_remove (lnode);

	  if (err)
	      goto exit;
	  
	  /* Got the node.  */
	  *np = node;
	  
	}
      else 
	{
	  /* The found node is not a directory.  */
	  mach_port_t p_restricted;

	  if (! lastcomp)
	    {
	      /* We have not reached the last path component yet.  */
	      port_dealloc (p);
	      err = ENOTDIR;
	      goto exit;
	    }

	  /* Check file permissions.  */
	  if (! err && (flags & O_READ))
	    err = fshelp_access (&statbuf, S_IREAD, user);
	  if (! err && (flags & O_WRITE))
	    err = fshelp_access (&statbuf, S_IWRITE, user);
	  if (! err && (flags & O_EXEC))
	    err = fshelp_access (&statbuf, S_IEXEC, user);

	  if (err)
	    {
	      port_dealloc (p);
	      goto exit;
	    }


	  /* A file node is successfully looked up.  */
	  err = io_restrict_auth (p, &p_restricted,
				  user->uids->ids, user->uids->num,
				  user->gids->ids, user->gids->num);
	  port_dealloc (p);
	  
	  if (err)
	    goto exit;
	  
	  /* Successfully restricted.  */
	  *port = p_restricted;
	  *port_type = MACH_MSG_TYPE_MOVE_SEND;
	}
    }
  
 exit: 

  if (err)
    *np = NULL;
  else if (*np)
    {
      mutex_unlock (&(*np)->lock);
      ncache_node_add (*np);
    }

  mutex_unlock (&dir->nn->lnode->lock);
  mutex_unlock (&dir->lock);
  return err;
}

/* We need our own, special implementation of netfs_S_dir_lookup,
   because libnetfs does not (yet?) know about cases, in which the
   servers wants to return (foreign) ports directly to the user,
   instead of usual node structures.  */

#define OPENONLY_STATE_MODES (O_CREAT|O_EXCL|O_NOLINK|O_NOTRANS|O_NONBLOCK)

fshelp_fetch_root_callback1_t _netfs_translator_callback1;
fshelp_fetch_root_callback2_t _netfs_translator_callback2;

error_t
netfs_S_dir_lookup (struct protid *diruser,
		    char *filename,
		    int flags,
		    mode_t mode,
		    retry_type *do_retry,
		    char *retry_name,
		    mach_port_t *retry_port,
		    mach_msg_type_name_t *retry_port_type)
{
  int create;			/* true if O_CREAT flag set */
  int excl;			/* true if O_EXCL flag set */
  int mustbedir = 0;		/* true if the result must be S_IFDIR */
  int lastcomp = 0;		/* true if we are at the last component */
  int newnode = 0;		/* true if this node is newly created */
  int nsymlinks = 0;
  struct node *dnp, *np;
  char *nextname;
  error_t error = 0;
  struct protid *newpi;
  struct iouser *user;

  if (!diruser)
    return EOPNOTSUPP;

  create = (flags & O_CREAT);
  excl = (flags & O_EXCL);

  /* Skip leading slashes */
  while (*filename == '/')
    filename++;

  *retry_port_type = MACH_MSG_TYPE_MAKE_SEND;
  *do_retry = FS_RETRY_NORMAL;
  *retry_name = '\0';

  if (*filename == '\0')
    {
      /* Set things up in the state expected by the code from gotit: on. */
      dnp = 0;
      np = diruser->po->np;
      mutex_lock (&np->lock);
      netfs_nref (np);
      goto gotit;
    }

  dnp = diruser->po->np;

  mutex_lock (&dnp->lock);

  netfs_nref (dnp);		/* acquire a reference for later netfs_nput */

  do
    {
      assert (!lastcomp);

      /* Find the name of the next pathname component */
      nextname = index (filename, '/');

      if (nextname)
	{
	  *nextname++ = '\0';
	  while (*nextname == '/')
	    nextname++;
	  if (*nextname == '\0')
	    {
	      /* These are the rules for filenames ending in /. */
	      nextname = 0;
	      lastcomp = 1;
	      mustbedir = 1;
	      create = 0;
	    }
	  else
	    lastcomp = 0;
	}
      else
	lastcomp = 1;

      np = 0;

    retry_lookup:

      if ((dnp == netfs_root_node || dnp == diruser->po->shadow_root)
	  && filename[0] == '.' && filename[1] == '.' && filename[2] == '\0')
	if (dnp == diruser->po->shadow_root)
	  /* We're at the root of a shadow tree.  */
	  {
	    *do_retry = FS_RETRY_REAUTH;
	    *retry_port = diruser->po->shadow_root_parent;
	    *retry_port_type = MACH_MSG_TYPE_COPY_SEND;
	    if (! lastcomp)
	      strcpy (retry_name, nextname);
	    error = 0;
	    mutex_unlock (&dnp->lock);
	    goto out;
	  }
	else if (diruser->po->root_parent != MACH_PORT_NULL)
	  /* We're at a real translator root; even if DIRUSER->po has a
	     shadow root, we can get here if its in a directory that was
	     renamed out from under it...  */
	  {
	    *do_retry = FS_RETRY_REAUTH;
	    *retry_port = diruser->po->root_parent;
	    *retry_port_type = MACH_MSG_TYPE_COPY_SEND;
	    if (!lastcomp)
	      strcpy (retry_name, nextname);
	    error = 0;
	    mutex_unlock (&dnp->lock);
	    goto out;
	  }
	else
	  /* We are global root */
	  {
	    error = 0;
	    np = dnp;
	    netfs_nref (np);
	  }
      else
	/* Attempt a lookup on the next pathname component. */
	error = netfs_attempt_lookup_improved (diruser->user, dnp,
					       filename, &np, 
					       flags, lastcomp,
					       retry_port, retry_port_type);
      
      /* At this point, DNP is unlocked */
      
      /* Implement O_EXCL flag here */
      if (lastcomp && create && excl && !error && np)
	error = EEXIST;
      
      /* Create the new node if necessary */
      if (lastcomp && create && error == ENOENT)
	{
	  mode &= ~(S_IFMT | S_ISPARE | S_ISVTX);
	  mode |= S_IFREG;
	  mutex_lock (&dnp->lock);

	  error = netfs_attempt_create_file_reduced (diruser->user, dnp,
						     filename, mode, flags);

	  /* We retry lookup in two cases:
	     - we created the file and we have to get a valid port;
	     - someone has already created the file (between our lookup
	     and this create) then we just got EEXIST.  If we are EXCL, 
	     that's fine; otherwise, we have to retry the lookup.  */
	    if ((!error) || (error == EEXIST && !excl))
	    {
	      mutex_lock (&dnp->lock);
	      goto retry_lookup;
	    }
	  
	  newnode = 1;
	}

      /* All remaining errors get returned to the user */
      if (error)
	goto out;

      if (np)
	{
	  mutex_lock (&np->lock);
	  error = netfs_validate_stat (np, diruser->user);
	  mutex_unlock (&np->lock);
	  if (error)
	    goto out;
	}

      if (np
	  && S_ISLNK (np->nn_translated)
	  && (!lastcomp
	      || mustbedir	/* "foo/" must see that foo points to a dir */
	      || !(flags & (O_NOLINK|O_NOTRANS))))
	{
	  size_t nextnamelen, newnamelen, linklen;
	  char *linkbuf;

	  /* Handle symlink interpretation */
	  if (nsymlinks++ > netfs_maxsymlinks)
	    {
	      error = ELOOP;
	      goto out;
	    }

	  linklen = np->nn_stat.st_size;

	  nextnamelen = nextname ? strlen (nextname) + 1 : 0;
	  newnamelen = nextnamelen + linklen + 1;
	  linkbuf = alloca (newnamelen);

	  error = netfs_attempt_readlink (diruser->user, np, linkbuf);
	  if (error)
	    goto out;

	  if (nextname)
	    {
	      linkbuf[linklen] = '/';
	      memcpy (linkbuf + linklen + 1, nextname,
		     nextnamelen - 1);
	    }
	  linkbuf[nextnamelen + linklen] = '\0';

	  if (linkbuf[0] == '/')
	    {
	      /* Punt to the caller */
	      *do_retry = FS_RETRY_MAGICAL;
	      *retry_port = MACH_PORT_NULL;
	      strcpy (retry_name, linkbuf);
	      goto out;
	    }

	  filename = linkbuf;
	  if (lastcomp)
	    {
	      lastcomp = 0;

	      /* Symlinks to nonexistent files aren't allowed to cause
		 creation, so clear the flag here. */
	      create = 0;
	    }
	  netfs_nput (np);
	  mutex_lock (&dnp->lock);
	  np = 0;
	}
      else
	{
	  /* Normal nodes here for next filename component */
	  filename = nextname;
	  netfs_nrele (dnp);

	  if (lastcomp)
	    dnp = 0;
	  else
	    {
	      dnp = np;
	      np = 0;
	    }
	}
    }
  while (filename && *filename);

  /* At this point, NP is the node to return.  */
 gotit:

  if (mustbedir && ! np)
    {
      error = ENOTDIR;
      goto out;
    }

  if (np)
    error = netfs_check_open_permissions (diruser->user, np,
					  flags, newnode);
      
  if (error)
    goto out;

  flags &= ~OPENONLY_STATE_MODES;

  if (np)
    {
      error = iohelp_dup_iouser (&user, diruser->user);
      if (error)
	goto out;

      newpi = netfs_make_protid (netfs_make_peropen (np, flags, diruser->po),
				 user);
      if (! newpi)
	{
	  iohelp_free_iouser (user);
	  error = errno;
	  goto out;
	}

      *retry_port = ports_get_right (newpi);
      ports_port_deref (newpi);
    }

 out:
  if (np)
    netfs_nput (np);
  if (dnp)
    netfs_nrele (dnp);
  return error;
}

/* Fill the array *DATA of size BUFSIZE with up to NENTRIES dirents
   from DIR (which is locked) starting with entry ENTRY for user CRED.
   The number of entries in the array is stored in *AMT and the number
   of bytes in *DATACNT.  If the supplied buffer is not large enough
   to hold the data, it should be grown.  */
error_t
netfs_get_dirents (struct iouser *cred, struct node *dir,
		   int first_entry, int num_entries, char **data,
		   mach_msg_type_number_t *data_len,
		   vm_size_t max_data_len, int *data_entries)
{
  node_dirent_t *dirent_start, *dirent_current;
  node_dirent_t *dirent_list = NULL;
  size_t size = 0;
  int count = 0;
  char *data_p;
  error_t err;

  int bump_size (const char *name)
    {
      if (num_entries == -1 || count < num_entries)
	{
	  size_t new_size = size + DIRENT_LEN (strlen (name));

	  if (max_data_len > 0 && new_size > max_data_len)
	    return 0;
	  size = new_size;
	  count++;
	  return 1;
	}
      else
	return 0;
    }

  int add_dirent (const char *name, ino_t fileno, int type)
    {
      if (num_entries == -1 || count < num_entries)
	{
	  struct dirent hdr;
	  size_t name_len = strlen (name);
	  size_t sz = DIRENT_LEN (name_len);

	  if (sz > size)
	    return 0;
	  else
	    size -= sz;
	  
	  hdr.d_fileno = fileno;
	  hdr.d_reclen = sz;
	  hdr.d_type = type;
	  hdr.d_namlen = name_len;

	  memcpy (data_p, &hdr, DIRENT_NAME_OFFS);
	  strcpy (data_p + DIRENT_NAME_OFFS, name);
	  data_p += sz;
	  count++;

	  return 1;
	}
      else
	return 0;
    }

  err = node_entries_get (dir, &dirent_list);

  if (! err)
    {
      for (dirent_start = dirent_list, count = 2;
	   dirent_start && first_entry > count;
	   dirent_start = dirent_start->next, count++);

      count = 0;

      /* Make space for the `.' and `..' entries.  */
      if (first_entry == 0)
	bump_size (".");
      if (first_entry <= 1)
	bump_size ("..");

      /* See how much space we need for the result.  */
      for (dirent_current = dirent_start;
	   dirent_current;
	   dirent_current = dirent_current->next)
	if (! bump_size (dirent_current->dirent->d_name))
	  break;

      *data = mmap (0, size, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
      err = ((void *) *data == (void *) -1) ? errno : 0;
    }

  if (! err)
    {
      data_p = *data;
      *data_len = size;
      *data_entries = count;
      count = 0;

      /* Add `.' and `..' entries.  */
      if (first_entry == 0)
	add_dirent (".", 2, DT_DIR);
      if (first_entry <= 1)
	add_dirent ("..", 2, DT_DIR);

      for (dirent_current = dirent_start;
	   dirent_current;
	   dirent_current = dirent_current->next)
	if (! add_dirent (dirent_current->dirent->d_name,
			  2	/* FIXME */,
			  dirent_current->dirent->d_type))
	  break;
    }

  if (dirent_list)
    node_entries_free (dirent_list);

  fshelp_touch (&dir->nn_stat, TOUCH_ATIME, maptime);

  return err;
}
