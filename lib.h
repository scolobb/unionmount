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

#ifndef INCLUDED_LIB_H
#define INCLUDED_LIB_H

#include <hurd.h>
#include <dirent.h>
#include <stddef.h>

/* Returned directory entries are aligned to blocks this many bytes
   long.  Must be a power of two.  */
#define DIRENT_ALIGN 4
#define DIRENT_NAME_OFFS offsetof (struct dirent, d_name)

/* Length is structure before the name + the name + '\0', all padded
   to a four-byte alignment.  */
#define DIRENT_LEN(name_len)                                 \
  ((DIRENT_NAME_OFFS + (name_len) + 1 + (DIRENT_ALIGN - 1))  \
   & ~(DIRENT_ALIGN - 1))

/* These macros remove some Mach specific code from the server
   itself.  */
#define port_null MACH_PORT_NULL
#define port_dealloc(p) mach_port_deallocate (mach_task_self (), (p))
#define port_valid(p) ((p) != port_null)

/* Fetch directory entries for DIR; store the raw data as returned by
   the dir_readdir RPC in *DIRENT_DATA, the size of *DIRENT_DATA in
   *DIRENT_DATA_SIZE and a list of pointers to the dirent structures
   in *DIRENT_LIST.  */
error_t dir_entries_get (file_t dir, char **dirent_data,
			 size_t *dirent_data_size,
			 struct dirent ***dirent_list);

char *make_filepath (char *, char *);
error_t for_each_subdir (char *, error_t (*) (char *, char *));
error_t for_each_subdir_priv (char *, error_t (*) (char *, char *, void *),
			      void *);

/* Lookup the file named NAME beneath DIR (or the cwd, if DIR is not a
   valid port.  Try to open with FLAGS0 first, and if that fails with
   FLAGS1; MODE is the mode to user for newly created files.  On
   success, stat the looked up port and store it in *PORT, the
   according stat information are stored in *STAT.  */
error_t file_lookup (file_t dir, char *name, int flags0, int flags1, int mode,
		     file_t *port, struct stat *stat);

/* Returns no error if directory.  */
error_t check_dir (char *path);

extern struct mutex debug_msg_lock;

/* Support for debugging messages.  */
#define debug_msg_send(fmt, args...)                         \
        do                                                   \
          {                                                  \
            mutex_lock (&debug_msg_lock);                    \
            fprintf (stderr, "%s:%i: ", __FILE__, __LINE__); \
            fprintf (stderr, fmt , ## args);                 \
            putc ('\n', stderr);                             \
            mutex_unlock (&debug_msg_lock);                  \
          }                                                  \
        while (0)

#endif
