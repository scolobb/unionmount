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

#include "mount.h"
#include "lib.h"
#include "ulfs.h"

/* The command line for starting the mountee.  */
char * mountee_argz;
size_t mountee_argz_len;

/* The node the mountee is sitting on.  */
node_t * mountee_node;

mach_port_t mountee_root;

int mountee_started = 0;

/* Starts the mountee (given by `argz` and `argz_len`), attaches it to
   the node `np` and opens a port `port` to the mountee with
   `flags`.  */
error_t
start_mountee (node_t * np, char * argz, size_t argz_len, int flags,
	       mach_port_t * port)
{
  error_t err;
  mach_port_t underlying_port;

  mach_port_t mountee_control;

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

  /* Attach the mountee to the port opened in the previous call.  */
  err = file_set_translator (underlying_port, 0, FS_TRANS_SET, 0, argz,
			     argz_len, mountee_control, MACH_MSG_TYPE_COPY_SEND);
  port_dealloc (underlying_port);
  if (err)
    return err;

  /* Obtain the port to the root of the newly-set translator.  */
  err = fsys_getroot (mountee_control, MACH_PORT_NULL, MACH_MSG_TYPE_COPY_SEND,
		      uids, nuids, gids, ngids, flags, &retry_port,
		      retry_name, port);
  return err;
}				/* start_mountee */

/* Sets up a proxy node, sets the translator on it, and registers the
   filesystem published by the translator in the list of merged
   filesystems.  */
error_t
setup_unionmount (void)
{
  error_t err = 0;

  /* The mountee will be sitting on this node.  This node is based on
     the netnode of the root node (it is essentially a clone of the
     root node), so most RPCs on this node can be automatically
     carried out correctly.  Note the we cannot set the mountee on the
     root node directly, because in this case the mountee's filesystem
     will obscure the filesystem published by unionfs.  */
  mountee_node = netfs_make_node (netfs_root_node->nn);
  if (!mountee_node)
    return ENOMEM;

  /* Set the mountee on the new node.
     Note that the O_READ flag does not actually limit access to the
     mountee's filesystem considerably.  Whenever a client looks up a
     node which is not a directory, unionfs will give off a port to
     the node itself, withouth proxying it.  Proxying happens only for
     directory nodes.  */
  err = start_mountee (mountee_node, mountee_argz, mountee_argz_len,
		       O_READ, &mountee_root);
  if (err)
    return err;

  /* A path equal to "" will mean that the current ULFS entry is the
     mountee port.  */
  ulfs_register ("", 0, 0);

  /* Reinitialize the list of merged filesystems to take into account
     the newly added mountee's filesystem.  */
  ulfs_check ();
  node_init_root (netfs_root_node);

  mountee_started = 1;

  return 0;
}				/* setup_unionmount */

