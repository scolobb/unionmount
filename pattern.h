/* Hurd unionfs
   Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.
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

/* Pattern list management.  */

#ifndef _PATTERN_H
#define _PATTERN_H

#include <hurd/netfs.h> /* For mutex stuff.  */

struct pattern
{
  char *pattern;

  struct pattern *next;
};

struct patternlist
{
  struct mutex lock;
  struct pattern *head;
};

/* Add a wildcard expression *PATTERN to **PATTERNLIST.  */
extern error_t patternlist_add (struct patternlist *list, char *pattern);

/* Check for match all pattern of the list *LIST, returning logical OR
   of results.  */
extern int patternlist_match (struct patternlist *list, char *string);

/* Free all resource used by *PATTERNLIST */
extern void patternlist_destroy (struct patternlist *list);

/* Return nonzero if *PATTERNLIST is empty */
extern int patternlist_isempty (struct patternlist *list);

#endif /* _PATTERN_H */
