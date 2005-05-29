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

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <error.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <stddef.h>

#include "lib.h"

/* Lock, which must be held, during printing of debugging
   messages.  */
struct mutex debug_msg_lock = MUTEX_INITIALIZER;

/* Returns no error if PATH points to a directory.  */
error_t check_dir (char *path)
{
  struct stat filestat;
  error_t err = 0;

  err = stat (path, &filestat);
  if (err)
    return err;

  if (!S_ISDIR (filestat.st_mode))
    return ENOTDIR;

  return 0;
}

/* Fetch directory entries for DIR; store the raw data as returned by
   the dir_readdir RPC in *DIRENT_DATA, the size of *DIRENT_DATA in
   *DIRENT_DATA_SIZE and a list of pointers to the dirent structures
   in *DIRENT_LIST.  */
error_t
dir_entries_get (file_t dir, char **dirent_data, 
		 size_t *dirent_data_size, struct dirent ***dirent_list)
{
  error_t err;
  size_t data_size;
  int entries_num;
  char *data;

  err = dir_readdir (dir, &data, &data_size, 0, -1, 0, &entries_num);
  if (! err)
    {
      struct dirent **list;
  
      list = malloc (sizeof (struct dirent *) * (entries_num + 1));
      if (list)
	{	
	  struct dirent *dp;
	  int i;

	  for (i = 0, dp = (struct dirent *) data;
	       i < entries_num;
	       i++, dp = (struct dirent *) ((char *) dp + dp->d_reclen))
	    *(list + i) = dp;
	  *(list + i) = NULL;

	  *dirent_data = data;
	  *dirent_data_size = data_size;
	  *dirent_list = list;
	}
      else
	{
	  munmap (data, data_size);
	  err = ENOMEM;
	}
    }
  return err;
}

/* Lookup the file named NAME beneath DIR (or the cwd, if DIR is not a
   valid port.  Try to open with FLAGS0 first, and if that fails with
   FLAGS1; MODE is the mode to user for newly created files.  On
   success, stat the looked up port and store it in *PORT, the
   according stat information are stored in *STAT.  */
error_t
file_lookup (file_t dir, char *name, int flags0, int flags1,
	     int mode, file_t *port, struct stat *stat)
{
  error_t err = 0;
  file_t p;
  struct stat s;

  file_t do_file_lookup (file_t d, char *n, int f, int m)
    {
      if (port_valid (d))
	p = file_name_lookup_under (d, n, f, m);
      else if (errno == EACCES)
	p = file_name_lookup (n, f, m);
      return p;
    }

  p = do_file_lookup (dir, name, flags0, mode);
  if (! port_valid (p))
    p = do_file_lookup (dir, name, flags1, mode);

  if (port_valid (p))
    {
      if (stat)
	{
	  err = io_stat (p, &s);
	  if (err)
	    port_dealloc (p);
	}
    }
  else
    err = errno;

  if (! err)
    {
      *port = p;
      if (stat)
	*stat = s;
    }
  return err;
}

#include <fcntl.h>

char *
make_filepath (char *path, char *filename)
{
  int length;
  char *filepath;

  length = strlen (path) + strlen (filename) + 2;
  filepath = malloc (length);
  if (filepath == NULL) 
    return NULL;
  
  strncpy (filepath, path, length);
  strncat (filepath, filename, strlen (filename));

  return filepath;
}

error_t
for_each_subdir (char *path, error_t (*func) (char *, char *))
{
  struct dirent **dirent, **dirent_list;
  char *dirent_data;
  size_t dirent_data_size;
  file_t dir;
  error_t err;

  dir = file_name_lookup (path, O_READ, 0);

  err = dir_entries_get (dir, &dirent_data, &dirent_data_size, &dirent_list);
  if (err)
    return err;

  for (dirent = dirent_list; (! err) && (*dirent); dirent++)
    {
      char *name;
      struct stat filestat;

      if ((!strcmp ((*dirent)->d_name, "."))
	  || (!strcmp ((*dirent)->d_name, "..")))
	continue;

      name = make_filepath (path, (*dirent)->d_name);

      err = stat (name, &filestat);

      if (err)
	return err;

      free (name);

      if (!S_ISDIR(filestat.st_mode))
	continue;
 
      err = func ((*dirent)->d_name, path);
    }

  return err;
}

error_t
for_each_subdir_priv (char *path, error_t (*func) (char *, char *, void *),
		      void *priv)
{
  struct dirent **dirent, **dirent_list;
  char *dirent_data;
  size_t dirent_data_size;
  file_t dir;
  error_t err;

  dir = file_name_lookup (path, O_READ, 0);

  err = dir_entries_get (dir, &dirent_data, &dirent_data_size, &dirent_list);
  if (err)
    return err;

  for (dirent = dirent_list; (!err) && (*dirent); dirent++)
    {
      char *name;
      struct stat filestat;

      if ((!strcmp ((*dirent)->d_name, "."))
	  || (!strcmp ((*dirent)->d_name, "..")))
	continue;

      name = make_filepath (path, (*dirent)->d_name);

      err = stat (name, &filestat);

      if (err)
	return err;

      free (name);

      if (!S_ISDIR(filestat.st_mode))
	continue;
 
      func ((*dirent)->d_name, path, priv);
    }

  return err;
}

error_t
for_each_file_priv (char *path, error_t (*func) (char *, char *, void *),
		    void *priv)
{
  struct dirent **dirent, **dirent_list;
  char *dirent_data;
  size_t dirent_data_size;
  file_t dir;
  error_t err;

  dir = file_name_lookup (path, O_READ, 0);

  err = dir_entries_get (dir, &dirent_data, &dirent_data_size, &dirent_list);
  if (err)
    return err;

  for (dirent = dirent_list; (!err) && (*dirent); dirent++)
    {

      if ((!strcmp ((*dirent)->d_name, "."))
	  || (!strcmp ((*dirent)->d_name, "..")))
	continue;

      func ((*dirent)->d_name, path, priv);
    }

  return err;
}
