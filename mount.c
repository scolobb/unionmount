/* Hurd unionmount
   The core of unionmount functionality.

   Copyright (C) 2009 Free Software Foundation, Inc.

   Written by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#define _GNU_SOURCE

#include <hurd/fsys.h>
#include <fcntl.h>
#include <cthreads.h>

#include "mount.h"
#include "lib.h"
#include "ulfs.h"

/* The command line for starting the mountee.  */
char * mountee_argz;
size_t mountee_argz_len;

mach_port_t mountee_root;
mach_port_t mountee_control = MACH_PORT_NULL;

int mountee_started = 0;

/* Shows the mode in which the current instance of unionmount
   operates (transparent/non-transparent).  */
int transparent_mount = 1;

/* Shows whether unionmount is shutting down.  */
int shutting_down = 0;

/* The port for receiving the notification about the shutdown of the
   mountee.  */
mach_port_t mountee_notify_port;

/* Starts the mountee (given by `argz` and `argz_len`), attaches it to
   the node `np` and opens a port `port` to the mountee with
   `flags`.  */
error_t
start_mountee (node_t * np, char * argz, size_t argz_len, int flags,
	       mach_port_t * port)
{
  error_t err;
  mach_port_t underlying_port;

  /* Identity information about the unionfs process (for
     fsys_getroot).  */
  uid_t * uids;
  size_t nuids;

  gid_t * gids;
  size_t ngids;

  /* The retry information returned by fsys_getroot.  */
  string_t retry_name;
  mach_port_t retry_port;

  /* Fetch the effective UIDs of the unionfs process.  */
  nuids = geteuids (0, 0);
  if (nuids < 0)
    return EPERM;
  uids = alloca (nuids * sizeof (uid_t));

  nuids = geteuids (nuids, uids);
  assert (nuids > 0);

  /* Fetch the effective GIDs of the unionfs process.  */
  ngids = getgroups (0, 0);
  if (ngids < 0)
    return EPERM;
  gids = alloca (ngids * sizeof (gid_t));

  ngids = getgroups (ngids, gids);
  assert (ngids > 0);

  /* Opens the port on which to set the mountee.  */
  error_t open_port (int flags, mach_port_t * underlying,
		     mach_msg_type_name_t * underlying_type, task_t task,
		     void *cookie)
  {
    err = 0;

    /* The protid which will contain the port to the node on which the
       mountee will be sitting.  */
    struct protid * newpi;

    struct iouser * unionfs_user;

    /* Initialize `unionfs_user` with the effective UIDs and GIDs of
       the unionfs process.  */
    err = iohelp_create_complex_iouser (&unionfs_user, uids, nuids, gids, ngids);
    if (err)
      return err;

    /* Create a port to node on which the mountee should sit (np).  */
    newpi = netfs_make_protid
      (netfs_make_peropen (np, flags, NULL), unionfs_user);
    if (!newpi)
      {
	iohelp_free_iouser (unionfs_user);
	return errno;
      }

    *underlying = underlying_port = ports_get_send_right (newpi);
    *underlying_type = MACH_MSG_TYPE_COPY_SEND;

    ports_port_deref (newpi);

    return err;
  }				/*open_port */

  /* Start the translator.  The value 60000 for the timeout is the one
     found in settrans.  */
  err = fshelp_start_translator (open_port, NULL, argz, argz, argz_len,
				 60000, &mountee_control);
  if (err)
    return err;

  /* Obtain the port to the root of the newly-set translator.  */
  err = fsys_getroot (mountee_control, MACH_PORT_NULL, MACH_MSG_TYPE_COPY_SEND,
		      uids, nuids, gids, ngids, flags, &retry_port,
		      retry_name, port);
  return err;
}				/* start_mountee */

/* Listens to the MACH_NOTIFY_DEAD_NAME notification for the port on
   the control port of the mountee.  */
error_t
mountee_server (mach_msg_header_t * inp, mach_msg_header_t * outp)
{
  /* If `shutting_down` is set, the mountee has just been shut down by
     netfs_shutdown and unionfs is on its way to going away.  */
  if (!shutting_down && (inp->msgh_id == MACH_NOTIFY_DEAD_NAME))
    {
      shutting_down = 1;

      /* Terminate operations conducted by unionfs and shut down.  */
      netfs_shutdown (FSYS_GOAWAY_FORCE);
      exit (0);
    }

  return 1;
}				/* mountee_server */

/* The main proc of the thread for listening for the
   MACH_NOTIFY_DEAD_NAME notification on the control port of the
   mountee.  */
static void
_mountee_listen_thread_proc (any_t * arg)
{
  while (1)
    mach_msg_server (mountee_server, 0, mountee_notify_port);
}

/* Sets up a proxy node, sets the translator on it, and registers the
   filesystem published by the translator in the list of merged
   filesystems.  */
error_t
setup_unionmount (void)
{
  error_t err = 0;

  /* Set the mountee on the root node.
     Note that the O_READ flag does not actually limit access to the
     mountee's filesystem considerably.  Whenever a client looks up a
     node which is not a directory, unionfs will give off a port to
     the node itself, withouth proxying it.  Proxying happens only for
     directory nodes.  */
  err = start_mountee (netfs_root_node, mountee_argz, mountee_argz_len,
		       O_READ, &mountee_root);
  if (err)
    return err;

  /* A path equal to "" will mean that the current ULFS entry is the
     mountee port.  */
  ulfs_register ("", 0, 0);

  mountee_started = 1;

  /* The previously registered send-once right (used to hold the value
     returned from mach_port_request_notification) */
  mach_port_t prev;

  /* Setup the port for receiving notifications.  */
  err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE,
		      &mountee_notify_port);
  if (err)
    return err;
  err = mach_port_insert_right (mach_task_self (), mountee_notify_port,
				mountee_notify_port, MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    {
      mach_port_deallocate (mach_task_self (), mountee_notify_port);
      return err;
    }

  /* Request to be notified when the mountee goes away and
     `mountee_control` becomes a dead name.  */
  err = mach_port_request_notification (mach_task_self (), mountee_control,
					MACH_NOTIFY_DEAD_NAME, 1, mountee_notify_port,
					MACH_MSG_TYPE_MAKE_SEND_ONCE, &prev);
  assert (prev == MACH_PORT_NULL);
  if(err)
    {
      mach_port_deallocate (mach_task_self(), mountee_notify_port);
      return err;
    }

  /* Create a new thread for listening to the notification.  */
  cthread_detach (cthread_fork ((cthread_fn_t) _mountee_listen_thread_proc, NULL));

  return err;
}				/* setup_unionmount */

