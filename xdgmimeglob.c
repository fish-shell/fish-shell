/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimeglob.c: Private file.  Datastructure for storing the globs.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 2.0
 * Or under the following terms:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xdgmimeglob.h"
#include "xdgmimeint.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fnmatch.h>

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

typedef struct XdgGlobHashNode XdgGlobHashNode;
typedef struct XdgGlobList XdgGlobList;

struct XdgGlobHashNode
{
  xdg_unichar_t character;
  const char *mime_type;
  XdgGlobHashNode *next;
  XdgGlobHashNode *child;
};
struct XdgGlobList
{
  const char *data;
  const char *mime_type;
  XdgGlobList *next;
};

struct XdgGlobHash
{
  XdgGlobList *literal_list;
  XdgGlobHashNode *simple_node;
  XdgGlobList *full_list;
};


/* XdgGlobList
 */
static XdgGlobList *
_xdg_glob_list_new (void)
{
  XdgGlobList *new_element;

  new_element = calloc (1, sizeof (XdgGlobList));

  return new_element;
}

/* Frees glob_list and all of it's children */
static void
_xdg_glob_list_free (XdgGlobList *glob_list)
{
  XdgGlobList *ptr, *next;

  ptr = glob_list;

  while (ptr != NULL)
    {
      next = ptr->next;

      if (ptr->data)
	free ((void *) ptr->data);
      if (ptr->mime_type)
	free ((void *) ptr->mime_type);
      free (ptr);

      ptr = next;
    }
}

static XdgGlobList *
_xdg_glob_list_append (XdgGlobList *glob_list,
		       void        *data,
		       const char  *mime_type)
{
  XdgGlobList *new_element;
  XdgGlobList *tmp_element;

  new_element = _xdg_glob_list_new ();
  new_element->data = data;
  new_element->mime_type = mime_type;
  if (glob_list == NULL)
    return new_element;

  tmp_element = glob_list;
  while (tmp_element->next != NULL)
    tmp_element = tmp_element->next;

  tmp_element->next = new_element;

  return glob_list;
}

#if 0
static XdgGlobList *
_xdg_glob_list_prepend (XdgGlobList *glob_list,
			void        *data,
			const char  *mime_type)
{
  XdgGlobList *new_element;

  new_element = _xdg_glob_list_new ();
  new_element->data = data;
  new_element->next = glob_list;
  new_element->mime_type = mime_type;

  return new_element;
}
#endif

/* XdgGlobHashNode
 */

static XdgGlobHashNode *
_xdg_glob_hash_node_new (void)
{
  XdgGlobHashNode *glob_hash_node;

  glob_hash_node = calloc (1, sizeof (XdgGlobHashNode));

  return glob_hash_node;
}

static void
_xdg_glob_hash_node_dump (XdgGlobHashNode *glob_hash_node,
			  int depth)
{
  int i;
  for (i = 0; i < depth; i++)
    printf (" ");

  printf ("%c", (char)glob_hash_node->character);
  if (glob_hash_node->mime_type)
    printf (" - %s\n", glob_hash_node->mime_type);
  else
    printf ("\n");
  if (glob_hash_node->child)
    _xdg_glob_hash_node_dump (glob_hash_node->child, depth + 1);
  if (glob_hash_node->next)
    _xdg_glob_hash_node_dump (glob_hash_node->next, depth);
}

static XdgGlobHashNode *
_xdg_glob_hash_insert_text (XdgGlobHashNode *glob_hash_node,
			    const char      *text,
			    const char      *mime_type)
{
  XdgGlobHashNode *node;
  xdg_unichar_t character;

  character = _xdg_utf8_to_ucs4 (text);

  if ((glob_hash_node == NULL) ||
      (character < glob_hash_node->character))
    {
      node = _xdg_glob_hash_node_new ();
      node->character = character;
      node->next = glob_hash_node;
      glob_hash_node = node;
    }
  else if (character == glob_hash_node->character)
    {
      node = glob_hash_node;
    }
  else
    {
      XdgGlobHashNode *prev_node;
      int found_node = FALSE;

      /* Look for the first character of text in glob_hash_node, and insert it if we
       * have to.*/
      prev_node = glob_hash_node;
      node = prev_node->next;

      while (node != NULL)
	{
	  if (character < node->character)
	    {
	      node = _xdg_glob_hash_node_new ();
	      node->character = character;
	      node->next = prev_node->next;
	      prev_node->next = node;

	      found_node = TRUE;
	      break;
	    }
	  else if (character == node->character)
	    {
	      found_node = TRUE;
	      break;
	    }
	  prev_node = node;
	  node = node->next;
	}

      if (! found_node)
	{
	  node = _xdg_glob_hash_node_new ();
	  node->character = character;
	  node->next = prev_node->next;
	  prev_node->next = node;
	}
    }

  text = _xdg_utf8_next_char (text);
  if (*text == '\000')
    {
      node->mime_type = mime_type;
    }
  else
    {
      node->child = _xdg_glob_hash_insert_text (node->child, text, mime_type);
    }
  return glob_hash_node;
}

static const char *
_xdg_glob_hash_node_lookup_file_name (XdgGlobHashNode *glob_hash_node,
				      const char      *file_name,
				      int              ignore_case)
{
  XdgGlobHashNode *node;
  xdg_unichar_t character;

  if (glob_hash_node == NULL)
    return NULL;

  character = _xdg_utf8_to_ucs4 (file_name);
  if (ignore_case)
    character = _xdg_ucs4_to_lower(character);

  for (node = glob_hash_node; node && character >= node->character; node = node->next)
    {
      if (character == node->character)
	{
	  file_name = _xdg_utf8_next_char (file_name);
	  if (*file_name == '\000')
	    return node->mime_type;
	  else
	    return _xdg_glob_hash_node_lookup_file_name (node->child,
							 file_name,
							 ignore_case);
	}
    }
  return NULL;
}

const char *
_xdg_glob_hash_lookup_file_name (XdgGlobHash *glob_hash,
				 const char  *file_name)
{
  XdgGlobList *list;
  const char *mime_type;
  const char *ptr;
  /* First, check the literals */

  assert (file_name != NULL);

  for (list = glob_hash->literal_list; list; list = list->next)
    if (strcmp ((const char *)list->data, file_name) == 0)
      return list->mime_type;

  ptr = strchr (file_name, '.');
  while (ptr != NULL)
    {
      mime_type = (_xdg_glob_hash_node_lookup_file_name (glob_hash->simple_node, ptr, FALSE));
      if (mime_type != NULL)
        return mime_type;
      
      mime_type = (_xdg_glob_hash_node_lookup_file_name (glob_hash->simple_node, ptr, TRUE));
      if (mime_type != NULL)
        return mime_type;
      
      ptr = strchr (ptr+1, '.');
    }

  /* FIXME: Not UTF-8 safe */
  for (list = glob_hash->full_list; list; list = list->next)
    if (fnmatch ((const char *)list->data, file_name, 0) == 0)
      return list->mime_type;

  return NULL;
}



/* XdgGlobHash
 */

XdgGlobHash *
_xdg_glob_hash_new (void)
{
  XdgGlobHash *glob_hash;

  glob_hash = calloc (1, sizeof (XdgGlobHash));

  return glob_hash;
}


static void
_xdg_glob_hash_free_nodes (XdgGlobHashNode *node)
{
  if (node)
    {
      if (node->child)
       _xdg_glob_hash_free_nodes (node->child);
      if (node->next)
       _xdg_glob_hash_free_nodes (node->next);
      if (node->mime_type)
	free ((void *) node->mime_type);
      free (node);
    }
}

void
_xdg_glob_hash_free (XdgGlobHash *glob_hash)
{
  _xdg_glob_list_free (glob_hash->literal_list);
  _xdg_glob_list_free (glob_hash->full_list);
  _xdg_glob_hash_free_nodes (glob_hash->simple_node);
  free (glob_hash);
}

XdgGlobType
_xdg_glob_determine_type (const char *glob)
{
  const char *ptr;
  int maybe_in_simple_glob = FALSE;
  int first_char = TRUE;

  ptr = glob;

  while (*ptr != '\000')
    {
      if (*ptr == '*' && first_char)
	maybe_in_simple_glob = TRUE;
      else if (*ptr == '\\' || *ptr == '[' || *ptr == '?' || *ptr == '*')
	  return XDG_GLOB_FULL;

      first_char = FALSE;
      ptr = _xdg_utf8_next_char (ptr);
    }
  if (maybe_in_simple_glob)
    return XDG_GLOB_SIMPLE;
  else
    return XDG_GLOB_LITERAL;
}

/* glob must be valid UTF-8 */
void
_xdg_glob_hash_append_glob (XdgGlobHash *glob_hash,
			    const char  *glob,
			    const char  *mime_type)
{
  XdgGlobType type;

  assert (glob_hash != NULL);
  assert (glob != NULL);

  type = _xdg_glob_determine_type (glob);

  switch (type)
    {
    case XDG_GLOB_LITERAL:
      glob_hash->literal_list = _xdg_glob_list_append (glob_hash->literal_list, strdup (glob), strdup (mime_type));
      break;
    case XDG_GLOB_SIMPLE:
      glob_hash->simple_node = _xdg_glob_hash_insert_text (glob_hash->simple_node, glob + 1, strdup (mime_type));
      break;
    case XDG_GLOB_FULL:
      glob_hash->full_list = _xdg_glob_list_append (glob_hash->full_list, strdup (glob), strdup (mime_type));
      break;
    }
}

void
_xdg_glob_hash_dump (XdgGlobHash *glob_hash)
{
  XdgGlobList *list;
  printf ("LITERAL STRINGS\n");
  if (glob_hash->literal_list == NULL)
    {
      printf ("    None\n");
    }
  else
    {
      for (list = glob_hash->literal_list; list; list = list->next)
	printf ("    %s - %s\n", (char *)list->data, list->mime_type);
    }
  printf ("\nSIMPLE GLOBS\n");
  _xdg_glob_hash_node_dump (glob_hash->simple_node, 4);

  printf ("\nFULL GLOBS\n");
  if (glob_hash->full_list == NULL)
    {
      printf ("    None\n");
    }
  else
    {
      for (list = glob_hash->full_list; list; list = list->next)
	printf ("    %s - %s\n", (char *)list->data, list->mime_type);
    }
}


void
_xdg_mime_glob_read_from_file (XdgGlobHash *glob_hash,
			       const char  *file_name)
{
  FILE *glob_file;
  char line[255];

  glob_file = fopen (file_name, "r");

  if (glob_file == NULL)
    return;

  /* FIXME: Not UTF-8 safe.  Doesn't work if lines are greater than 255 chars.
   * Blah */
  while (fgets (line, 255, glob_file) != NULL)
    {
      char *colon;
      if (line[0] == '#')
	continue;

      colon = strchr (line, ':');
      if (colon == NULL)
	continue;
      *(colon++) = '\000';
      colon[strlen (colon) -1] = '\000';
      _xdg_glob_hash_append_glob (glob_hash, colon, line);
    }

  fclose (glob_file);
}
