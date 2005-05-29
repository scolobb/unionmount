/* stow-mutations.h - MIG mutations unionfs.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Gianluca Guida <glguida@gmail.com>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

/* Only CPP macro definitions should go in this file. */

#define FS_NOTIFY_INTRAN stow_notify_t begin_using_notify_port (fs_notify_t)
#define FS_NOTIFY_DESTRUCTOR end_using_notify_port (stow_notify_t)

#define FS_NOTIFY_IMPORTS import "stow-priv.h";

