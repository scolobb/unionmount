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

#ifndef __STOW_PRIVDATA_H__
#define __STOW_PRIVDATA_H__

#include <hurd/ports.h>

struct stow_notify
{
  struct port_info pi;

  char *dir_name;
  struct stow_privdata *priv;
};
typedef struct stow_notify *stow_notify_t;


/* Called by MiG to translate ports into stow_notify_t.  mutations.h
   arranges for this to happen for the fs_notify interfaces. */
stow_notify_t begin_using_notify_port (fs_notify_t port);


/* Called by MiG after server routines have been run; this balances
   begin_using_notify_port, and is arranged for the fs_notify
   interfaces by mutations.h. */
void end_using_notify_port (stow_notify_t cred);

#endif /* STOW_PRIVDATA_H */
