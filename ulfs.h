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

#ifndef INCLUDED_ULFS_H
#define INCLUDED_ULFS_H

/* The structure for each registered underlying filesystem.  */
typedef struct ulfs
{
  char *path;
  int flags;
  struct ulfs *next, *prev, **prevp;
} ulfs_t;

/* Flags.  */

/* The according ulfs is marked writable.  */
#define FLAG_ULFS_WRITABLE 0x00000001

/* The start of the ulfs chain.  */
extern ulfs_t *ulfs_chain_start;

/* The end of the ulfs chain, we need this, to go through the chain in
   reversed order.  */
extern ulfs_t *ulfs_chain_end;

/* Number of registered underlying filesystems.  */
extern unsigned int ulfs_num;

/* The lock protecting the ulfs data structures.  */
extern struct mutex ulfs_lock;

/* Register a new underlying filesystem.  */
error_t ulfs_register (char *path, int flags);

/* Unregister an underlying filesystem.  */
error_t ulfs_unregister (char *path);

/* Get an ULFS element by it's index.  */
error_t ulfs_get_num (int num, ulfs_t **ulfs);

/* Get an ulfs element by the associated path.  */
error_t ulfs_get_path (char *path, ulfs_t **ulfs);

#define ulfs_iterate                             \
  for (ulfs_t *ulfs = (mutex_lock (&ulfs_lock),  \
		       ulfs_chain_end);          \
       ulfs || (mutex_unlock (&ulfs_lock), 0);   \
       ulfs = ulfs->prev) 

#define ulfs_iterate_unlocked                    \
  for (ulfs_t *ulfs = ulfs_chain_end;            \
       ulfs;                                     \
       ulfs = ulfs->prev)

#endif
