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

/* Argument parsing.  */

#define _GNU_SOURCE

#include <argp.h>
#include <error.h>

#include "options.h"
#include "ulfs.h"
#include "ncache.h"
#include "unionfs.h"
#include "node.h"
#include "version.h"

/* This variable is set to a non-zero value after parsing of the
   startup options.  Whenever the argument parser is later called to
   modify the underlying filesystems of the root node, the root node
   is initialized accordingly directly by the parser.  */
int parsing_startup_options_finished;

/* Argp options common to the runtime and startup parser.  */
const struct argp_option argp_common_options[] =
  {
    { OPT_LONG_UNDERLYING, OPT_UNDERLYING, 0, 0,
      "add the underlying node to the unionfs" },
    { OPT_LONG_WRITABLE, OPT_WRITABLE, 0, 0,
      "specify the following filesystem as writable" },
    { OPT_LONG_DEBUG, OPT_DEBUG, 0, OPTION_HIDDEN,
      "send debugging messages to stderr" },
    { OPT_LONG_CACHE_SIZE, OPT_CACHE_SIZE, "SIZE", 0,
      "specify the maximum number of nodes in the cache" },
    { 0, 0, 0, 0, "Runtime options:", 1 },
    { OPT_LONG_REMOVE, OPT_REMOVE, 0, 0,
      "remove the following filesystem", 1 },
    { 0 }
  };

/* Argp options only meaningful for startup parsing.  */
const struct argp_option argp_startup_options[] =
  {
    { 0 }
  };

/* Argp parser function for the common oprtions.  */
error_t
argp_parse_common_options (int key, char *arg, struct argp_state *state)
{
  static int ulfs_flags = 0, ulfs_remove = 0, ulfs_modified = 0;
  error_t err = 0;

  switch (key)
    {
    case OPT_WRITABLE:		/* --writable  */
      ulfs_flags |= FLAG_ULFS_WRITABLE;
      break;

    case OPT_DEBUG:		/* --debug  */
      unionfs_flags |= FLAG_UNIONFS_MODE_DEBUG;
      break;

    case OPT_CACHE_SIZE:	/* --cache-size  */
      ncache_size = strtol (arg, NULL, 10);
      break;

    case OPT_REMOVE:		/* --remove  */
      ulfs_remove = 1;
      break;

    case OPT_UNDERLYING:	/* --underlying  */
    case ARGP_KEY_ARG:
      if (ulfs_remove)
	{
	  err = ulfs_unregister (arg);
	  if (err == ENOENT)
	    /* It is not a fatal error, when the user tries to remove
	       a filesystem, which is not used by unionfs.  */
	    err = 0;
	}
      else
	err = ulfs_register (arg, ulfs_flags);
      if (err)
	error (EXIT_FAILURE, err, "ulfs_register");
      ulfs_modified = 1;
      ulfs_flags = ulfs_remove = 0;
      break;

    case ARGP_KEY_END:
      ulfs_flags = ulfs_remove = 0;
      if (ulfs_modified && parsing_startup_options_finished)
	{
	  err = node_init_root (netfs_root_node);
	  if (err)
	    error (EXIT_FAILURE, err, "failed to initialize root node");
	}
      ncache_reset ();
      ulfs_modified = 0;
      if (! parsing_startup_options_finished)
	parsing_startup_options_finished = 1;
      break;

    default:
      err = ARGP_ERR_UNKNOWN;
      break;
    }

  return err;
}

/* Argp parser function for the startup oprtions.  */
error_t
argp_parse_startup_options (int key, char *arg, struct argp_state *state)
{
  error_t err = 0;

  switch (key)
    {
    default:
      err = ARGP_ERR_UNKNOWN;
      break;
    }
  
  return err;
}

/* Argp parser for only the common options.  */
const struct argp argp_parser_common_options =
  { argp_common_options, argp_parse_common_options, 0, 0, 0 };

/* Argp parser for only the startup options.  */
struct argp argp_parser_startup_options =
  { argp_startup_options, argp_parse_startup_options, 0, 0, 0 };

/* The children parser for runtime arguments.  */
const struct argp_child argp_children_runtime[] =
  {
    { &argp_parser_common_options },
    { &netfs_std_runtime_argp },
    { 0 }
  };

/* The children parser for startup arguments.  */
const struct argp_child argp_children_startup[] =
  {
    { &argp_parser_startup_options },
    { &argp_parser_common_options },
    { &netfs_std_startup_argp },
    { 0 }
  };

const char *argp_program_version = STANDARD_HURD_VERSION (unionfs);
const char *argp_program_bug_address = 
"Moritz Schulte <moritz@duesseldorf.ccc.de>";

#define ARGS_DOC "FILESYSTEMS ..."
#define DOC      "Hurd unionfs server"

/* The final argp parser for runtime arguments.  */
struct argp argp_runtime =
  { 0, 0, 0, 0, argp_children_runtime };

/* The final argp parser for startup arguments.  */
struct argp argp_startup =
  { 0, 0, ARGS_DOC, DOC, argp_children_startup };