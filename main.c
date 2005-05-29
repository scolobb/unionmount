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
#include <argp.h>
#include <argz.h>
#include <error.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "version.h"
#include "unionfs.h"
#include "ncache.h"
#include "ulfs.h"
#include "lnode.h"
#include "node.h"
#include "options.h"
#include "stow.h"
#include "update.h"

char *netfs_server_name = "unionfs";
char *netfs_server_version = HURD_VERSION;

/* Not really needed, since unionfs doesn't manage symlinks.  */
int netfs_maxsymlinks = 0;

/* Flags describing certain properties of the unionfs.  */
int unionfs_flags;

/* The filesystem id (the pid).  */
pid_t fsid;

/* A port to the underlying node.  */
mach_port_t underlying_node;

/* stat information for the underlying node.  */
struct stat underlying_node_stat;

/* Mapped time, used for updating node information.  */
volatile struct mapped_time_value *maptime;

/* Used by netfs_set_options to handle runtime option parsing.  */
struct argp *netfs_runtime_argp = &argp_runtime;

/* Main entry point.  */
int
main (int argc, char **argv)
{
  mach_port_t bootstrap_port;
  error_t err = 0;

  root_update_init ();

  err = stow_init();
  if (err)
    error (EXIT_FAILURE, err, "failed to initialize stow support");

  /* Argument parsing.  */
  argp_parse (&argp_startup, argc, argv, ARGP_IN_ORDER, 0, 0);

  err = node_create_root (&netfs_root_node);
  if (err)
    error (EXIT_FAILURE, err, "failed to create root node");

  /* netfs initialization.  */
  task_get_bootstrap_port (mach_task_self (), &bootstrap_port);
  netfs_init ();
  underlying_node = netfs_startup (bootstrap_port, O_READ);

  err = node_init_root (netfs_root_node);
  if (err)
    error (EXIT_FAILURE, err, "failed to initialize root node");

  /* Map the time, used for updating node information.  */
  err = maptime_map (0, 0, &maptime);
  if (err)
    error (EXIT_FAILURE, err, "maptime_map");

  /* More initialiazation.  */
  ncache_init (ncache_size);

  /* Here we adjust the root node permissions.  */
  err = io_stat (underlying_node, &underlying_node_stat);

  if (err)
    error (EXIT_FAILURE, err, "io_stat");

  fsid = getpid ();
  netfs_root_node->nn_stat = underlying_node_stat;
  netfs_root_node->nn_stat.st_ino = UNIONFS_ROOT_INODE; /* FIXME.  */
  netfs_root_node->nn_stat.st_fsid = fsid;
  netfs_root_node->nn_stat.st_mode = S_IFDIR | (underlying_node_stat.st_mode
						& ~S_IFMT & ~S_ITRANS);
  netfs_root_node->nn_translated = netfs_root_node->nn_stat.st_mode;
  
  /* If the underlying node isn't a directory, enhance the stat
     information.  */
  if (! S_ISDIR (underlying_node_stat.st_mode))
    {
      if (underlying_node_stat.st_mode & S_IRUSR)
	netfs_root_node->nn_stat.st_mode |= S_IRUSR;
      if (underlying_node_stat.st_mode & S_IRGRP)
	netfs_root_node->nn_stat.st_mode |= S_IRGRP;
      if (underlying_node_stat.st_mode & S_IROTH)
	netfs_root_node->nn_stat.st_mode |= S_IXOTH;
    }

  /* Update the timestamps of the root node.  */
  fshelp_touch (&netfs_root_node->nn_stat,
		TOUCH_ATIME | TOUCH_MTIME | TOUCH_CTIME, maptime);

  /* Start serving clients.  */
  for (;;)
    netfs_server_loop ();
}
