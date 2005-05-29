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

/* Update thread: A clean way to solve locking issues of 
   root node update.  */

#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <cthreads.h>
#include <rwlock.h>

#include "ncache.h"
#include "node.h"
#include "ulfs.h"

/* Reader lock is used by threads that are going to
   add/remove an ulfs; writer lock is hold by the 
   update thread.  */
static struct rwlock update_rwlock;
static struct condition update_wakeup;
static struct mutex update_lock;

static void
_root_update_thread ()
{
  error_t err;
  
  while (1)
    {
      if (hurd_condition_wait (&update_wakeup, &update_lock))
	mutex_unlock (&update_lock);

      rwlock_writer_lock (&update_rwlock);

      do 
	{
	  ulfs_check();
	  err = node_init_root (netfs_root_node);
	}
      while (err == ENOENT);

      if (err)
	{
	  fprintf (stderr, "update thread: got a %s\n", strerror (err));
	}

      ncache_reset ();

      rwlock_writer_unlock (&update_rwlock);
    }
}

void
root_update_schedule ()
{
  condition_signal (&update_wakeup);
}

void
root_update_disable ()
{
  rwlock_reader_lock (&update_rwlock);
}

void
root_update_enable ()
{
  rwlock_reader_unlock (&update_rwlock);
}

void
root_update_init()
{
  mutex_init (&update_lock);
  rwlock_init (&update_rwlock);
  condition_init (&update_wakeup);

  cthread_detach (cthread_fork ( (cthread_fn_t)_root_update_thread, 0));
}
