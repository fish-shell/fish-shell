/** \file xdgmimeparent.h
    X Desktop Group Multipurpose Internet Mail Extensions heirarchy header.
*/

/* xdgmimeparent.h: Private file.  Datastructure for storing the hierarchy.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2004  Red Hat, Inc.
 * Copyright (C) 200  Matthias Clasen <mclasen@redhat.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __XDG_MIME_PARENT_H__
#define __XDG_MIME_PARENT_H__

#include "xdgmime.h"

typedef struct XdgParentList XdgParentList;

#ifdef XDG_PREFIX
#define _xdg_mime_parent_read_from_file        XDG_ENTRY(parent_read_from_file)
#define _xdg_mime_parent_list_new              XDG_ENTRY(parent_list_new)
#define _xdg_mime_parent_list_free             XDG_ENTRY(parent_list_free)
#define _xdg_mime_parent_list_lookup           XDG_ENTRY(parent_list_lookup)
#endif

void          _xdg_mime_parent_read_from_file(XdgParentList *list,
        const char    *file_name);
XdgParentList *_xdg_mime_parent_list_new(void);
void           _xdg_mime_parent_list_free(XdgParentList *list);
const char   **_xdg_mime_parent_list_lookup(XdgParentList *list,
        const char    *mime);
void           _xdg_mime_parent_list_dump(XdgParentList *list);

#endif /* __XDG_MIME_PARENT_H__ */
