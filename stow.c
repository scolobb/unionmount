/* Hurd unionfs
   Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.
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

struct stow_privdata
{
  struct patternlist *patternlist;
  int flags;
  int remove;
};

static error_t
_stow_registermatchingdirs (char *arg, char *dirpath, void *priv)
{
  error_t err=0;
  char *filepath;

  struct stow_privdata *privdata = (struct stow_privdata *) priv ;

  err = patternlist_match (privdata->patternlist, arg);

  if (err)
    return err;

  filepath = make_filepath (dirpath, arg);

  debug_msg_send ("adding %s\n", filepath);


  if (privdata->remove)
    {
      err = ulfs_unregister (filepath);
      if (err == ENOENT)
	/* It is not a fatal error, when the user tries to remove
	   a filesystem, which is not used by unionfs.  */
	err = 0;
    }
  else
    err = ulfs_register (filepath, privdata->flags);
  if (err)
    error (EXIT_FAILURE, err, "ulfs_register");
  
  free (filepath);

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

  if (patternlist_isempty (privdata->patternlist))
    {
      if (privdata->remove)
	{
	  err = ulfs_unregister (filepath);
	  if (err == ENOENT)
	    /* It is not a fatal error, when the user tries to remove
	       a filesystem, which is not used by unionfs.  */
	    err = 0;
	}
      else
	err = ulfs_register (filepath, privdata->flags);
      if (err)
	error (EXIT_FAILURE, err, "ulfs_register");      
    }
  else
    {
      err = for_each_subdir_priv (filepath, _stow_registermatchingdirs, priv);
    }

  free (filepath);
   
  return err;
}

error_t
stow_diradd (char *dir, int flags, struct patternlist *patternlist, int remove)
{

  error_t err;
  struct stow_privdata mypriv = 
    { 
      .patternlist = patternlist, 
      .flags = flags,
      .remove = remove
    };

  err = for_each_subdir_priv (dir, _stow_scanstowentry, (void *)&mypriv);

  return err;

}
