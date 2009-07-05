/* Hurd unionmount
   General information and properties for unionmount/unionfs.

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

#ifndef INCLUDED_MOUNT_H
#define INCLUDED_MOUNT_H

#include <unistd.h>

/* The command line for starting the mountee.  */
extern char * mountee_argz;
extern size_t mountee_argz_len;

#endif /* not INCLUDED_MOUNT_H */
