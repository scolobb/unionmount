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

#define _GNU_SOURCE

#include <hurd/netfs.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <errno.h>

#include "pattern.h"

/* Add a wildcard expression *PATTERN to **PATTERNLIST.  */
error_t
patternlist_add (struct patternlist *list, char *pattern)
{
  error_t err = 0;
  struct pattern *listentry;
  char *dup;

  if (pattern == NULL) /* BUG.  */
    err = EINVAL;
  
  if (err)
    return err;

  dup = strdup (pattern);
  if (dup == NULL) 
    err = ENOMEM;

  if (err)
      return err;

  listentry = malloc (sizeof (struct pattern));
  if (listentry == NULL) 
    err = ENOMEM;

  if (err)
      return err;

  listentry->pattern = dup;

  mutex_lock (& (list->lock));
  if (list->head == NULL) /* List is empty.  */
    {
      list->head = listentry;
      listentry->next = NULL;
    }
  else
    {
      listentry->next = list->head;
      list->head = listentry;
    }
  mutex_unlock (& (list->lock));

  return err;
}

/* Check for match all pattern of the list *LIST, returning logical OR
   of results.  */
int
patternlist_match (struct patternlist *list, char *string)
{
  struct pattern *ptr;
  error_t err = ~0; /* Return false by default */

  ptr = list->head;

  mutex_lock (&list->lock);
  while (ptr != NULL)
    {
      err = fnmatch (ptr->pattern, string, FNM_FILE_NAME);

      if (!err) /* String matched.  */
	break;

      ptr = ptr->next;
    }
  mutex_unlock (&list->lock);

  return err;
}

/* Free all resource used by *PATTERNLIST.  */
void
patternlist_destroy (struct patternlist *list)
{
  struct pattern *next, *ptr = list->head;

  mutex_lock (& (list->lock));
  while (ptr != NULL)
    {
      next = ptr->next;

      free (ptr);

      ptr = next;
    }
  mutex_unlock (& (list->lock));
}

/* Return nonzero if *PATTERNLIST is empty.  */
int
patternlist_isempty (struct patternlist *list)
{
  int ret;

  mutex_lock (& (list->lock));
  ret = (list->head == NULL);
  mutex_unlock (& (list->lock));

  return ret;
}
