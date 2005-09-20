/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmime.h: XDG Mime Spec mime resolver.  Based on version 0.11 of the spec.
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


#ifndef __XDG_MIME_H__
#define __XDG_MIME_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef XDG_PREFIX
#define XDG_ENTRY(func) _XDG_ENTRY2(XDG_PREFIX,func)
#define _XDG_ENTRY2(prefix,func) _XDG_ENTRY3(prefix,func)
#define _XDG_ENTRY3(prefix,func) prefix##_##func
#endif

typedef void (*XdgMimeCallback) (void *user_data);
typedef void (*XdgMimeDestroy)  (void *user_data);

  
#ifdef XDG_PREFIX
#define xdg_mime_get_mime_type_for_data       XDG_ENTRY(get_mime_type_for_data)
#define xdg_mime_get_mime_type_for_file       XDG_ENTRY(get_mime_type_for_file)
#define xdg_mime_get_mime_type_from_file_name XDG_ENTRY(get_mime_type_from_file_name)
#define xdg_mime_is_valid_mime_type           XDG_ENTRY(is_valid_mime_type)
#define xdg_mime_mime_type_equal              XDG_ENTRY(mime_type_equal)
#define xdg_mime_media_type_equal             XDG_ENTRY(media_type_equal)
#define xdg_mime_mime_type_subclass           XDG_ENTRY(mime_type_subclass)
#define xdg_mime_get_mime_parents             XDG_ENTRY(get_mime_parents)
#define xdg_mime_unalias_mime_type            XDG_ENTRY(unalias_mime_type)
#define xdg_mime_get_max_buffer_extents       XDG_ENTRY(get_max_buffer_extents)
#define xdg_mime_shutdown                     XDG_ENTRY(shutdown)
#define xdg_mime_register_reload_callback     XDG_ENTRY(register_reload_callback)
#define xdg_mime_remove_callback              XDG_ENTRY(remove_callback)
#define xdg_mime_type_unknown                 XDG_ENTRY(type_unknown)
#endif

extern const char *xdg_mime_type_unknown;
#define XDG_MIME_TYPE_UNKNOWN xdg_mime_type_unknown

const char  *xdg_mime_get_mime_type_for_data       (const void *data,
						    size_t      len);
const char  *xdg_mime_get_mime_type_for_file       (const char *file_name);
const char  *xdg_mime_get_mime_type_from_file_name (const char *file_name);
int          xdg_mime_is_valid_mime_type           (const char *mime_type);
int          xdg_mime_mime_type_equal              (const char *mime_a,
						    const char *mime_b);
int          xdg_mime_media_type_equal             (const char *mime_a,
						    const char *mime_b);
int          xdg_mime_mime_type_subclass           (const char *mime_a,
						    const char *mime_b);
const char **xdg_mime_get_mime_parents		   (const char *mime);
const char  *xdg_mime_unalias_mime_type		   (const char *mime);
int          xdg_mime_get_max_buffer_extents       (void);
void         xdg_mime_shutdown                     (void);
void         xdg_mime_dump                         (void);
int          xdg_mime_register_reload_callback     (XdgMimeCallback  callback,
						    void            *data,
						    XdgMimeDestroy   destroy);
void         xdg_mime_remove_callback              (int              callback_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __XDG_MIME_H__ */
