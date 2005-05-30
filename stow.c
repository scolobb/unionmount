/* Hurd unionfs
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Gianluca Guida <glguida@gmail.com>.

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


/* Stow mode for unionfs.  */

#define _GNU_SOURCE

#include <argp.h>
#include <error.h>

#include "ulfs.h"
#include "lib.h"
#include "pattern.h"
#include "update.h"

struct stow_privdata
{
  struct patternlist *patternlist;
  int flags;
  struct mutex lock;
};

static error_t
_stow_registermatchingdirs (char *arg, char *dirpath, void *priv)
{
  error_t err = 0;
  char *filepath;

  struct stow_privdata *privdata = (struct stow_privdata *) priv ;

  err = patternlist_match (privdata->patternlist, arg);
  if (err)
    return 0; /* It doesn't match. This is not an error.  */

  filepath = make_filepath (dirpath, arg);

  err = ulfs_register (filepath, privdata->flags);

  free (filepath);

  if (err)
    return err;
  
  return 0;
}

static error_t
_stow_scanstowentry (char *arg, char *dirpath, void *priv)
{
  char *filepath = dirpath;
  error_t err;

  struct stow_privdata *privdata = (struct stow_privdata *) priv ;

  if (dirpath) 
    {
      char *tmp;
      tmp = make_filepath (dirpath, arg);
      filepath = make_filepath (tmp, "/");
      free (tmp);
    }

  mutex_lock (&privdata->lock);

  if (patternlist_isempty (privdata->patternlist))
    {

      err = ulfs_register (filepath, privdata->flags);
      if (err)
	{
	  mutex_unlock (&privdata->lock);
	  return err;
	}

    }
  else
    {
      err = for_each_subdir_priv (filepath, _stow_registermatchingdirs, priv);
      if (err)
	{
	  mutex_unlock (&privdata->lock);
	  free (filepath);
	  return err;
	}
    }

  free (filepath);
  mutex_unlock (&privdata->lock);
  return err;
}


/* Implement server for fs_notify.  */

#include <cthreads.h>
#include <hurd/port.h>

#include "stow-priv.h"
#include "ncache.h"

struct port_bucket *stow_port_bucket;
struct port_class *stow_port_class;

static error_t
_stow_notify_init(char *dir_name, void *priv)
{
  error_t err;
  file_t dir_port;
  mach_port_t notify_port;
  stow_notify_t stow_notify_port;

  err = ports_create_port (stow_port_class, stow_port_bucket,
			   sizeof (*stow_notify_port),
			   &stow_notify_port);
  if (err)
    return err;

  stow_notify_port->dir_name = dir_name;
  stow_notify_port->priv = priv;

  dir_port = file_name_lookup (dir_name, 0, 0); 

  if (!port_valid (dir_port))
      return ENOENT; /* ? */


  notify_port = ports_get_right (stow_notify_port);

  if (!port_valid (notify_port))
    {
      port_dealloc (dir_port);
      return EACCES; /* ? */
    }

  err = dir_notice_changes (dir_port, notify_port, 
			    MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    {
      port_dealloc (dir_port);
      port_dealloc (notify_port);
      return err;
    }

  return err;
}

/* Called by MiG to translate ports into stow_notify_t.  mutations.h
   arranges for this to happen for the fs_notify interfaces. */
stow_notify_t
begin_using_notify_port (fs_notify_t port)
{
  return ports_lookup_port (stow_port_bucket, port, stow_port_class);
}

/* Called by MiG after server routines have been run; this balances
   begin_using_notify_port, and is arranged for the fs_notify
   interfaces by mutations.h. */
void
end_using_notify_port (stow_notify_t cred)
{
  if (cred)
    ports_port_deref (cred);
}

/* We don't ask for file_changes, but this function has to be defined .  */
kern_return_t
stow_S_file_changed (stow_notify_t notify, natural_t tickno,
		     file_changed_type_t change, loff_t start, 
		     loff_t end)
{
  return EOPNOTSUPP;
}

/* Called when we receive a dir_changed message.  */
kern_return_t
stow_S_dir_changed (stow_notify_t notify, natural_t tickno,
		    dir_changed_type_t change, string_t name)
{
  error_t err;

  if (!notify || !notify->dir_name || !notify->priv)
    return EOPNOTSUPP;

  switch (change)
    {
    case DIR_CHANGED_NULL:
      break;
    case DIR_CHANGED_NEW:
      root_update_disable ();

      err = _stow_scanstowentry (name, notify->dir_name, notify->priv);
      if (err)
	  debug_msg_send ("scanstowentry: %s\n", strerror (err));

      root_update_schedule ();
      root_update_enable ();
      break;

    case DIR_CHANGED_UNLINK:
      root_update_schedule ();
      break;

    default:
      debug_msg_send ("unsupported dir change notify");
      return EINVAL;
    }

  return 0;
}

/* This is the server thread waiting for dir_changed messages.  */
static void
_stow_notify_thread()
{
  int stow_demuxer (mach_msg_header_t *inp, mach_msg_header_t *outp)
    {
      int stow_fs_notify_server (mach_msg_header_t *inp,
				 mach_msg_header_t *outp);
      
      return (stow_fs_notify_server (inp, outp));
    }
  
  do
    {
      ports_manage_port_operations_multithread (stow_port_bucket,
						 stow_demuxer,
						 1000 * 60 * 2,
						 1000 * 60 * 10,
						 0);
    }
  while (1);
}



/* Interface to unionfs.  */

error_t
stow_diradd (char *dir, int flags, struct patternlist *patternlist)
{

  error_t err;
  struct stow_privdata *mypriv;
  int dir_len;

  dir_len = strlen(dir);
  if (dir_len == 0)
    {
      return EOPNOTSUPP;
    }

  if (dir[dir_len - 1 ] != '/')
    {
      char *tmp;

      tmp = (char *) malloc (dir_len + 1);

      if (tmp == NULL)
	return ENOMEM;

      strncpy (tmp, dir, dir_len);

      tmp[dir_len] = '/';

      dir = tmp;
    }

  mypriv = malloc (sizeof (struct stow_privdata));
  if (!mypriv)
    {
      free (dir);
      return ENOMEM;
    }

  mypriv->patternlist = patternlist;
  mypriv->flags = flags;
  mutex_init (&mypriv->lock);
  
  err = for_each_subdir_priv (dir, _stow_scanstowentry, (void *)mypriv);
  if (err)
    {
      /* FIXME: rescan and delete previous inserted things.  */
      return err;
    }

  err = _stow_notify_init (dir, mypriv);
  assert (!err);

  return err;
}

error_t
stow_init (void)
{
  error_t err = 0;

  stow_port_bucket = ports_create_bucket ();
  if (!stow_port_bucket)
    return errno;

  stow_port_class = ports_create_class (NULL, NULL);
  if (!stow_port_class)
    return errno;

  cthread_detach (cthread_fork ( (cthread_fn_t)_stow_notify_thread, 0));

  return err;
}
