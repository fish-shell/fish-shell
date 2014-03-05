/** \file xdgmime.cpp
    X Desktop Group Multipurpose Internet Mail Extensions resolver.
*/

/* xdgmime.cpp: XDG Mime Spec mime resolver.  Based version 0.11 of the spec.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2003,2004  Red Hat, Inc.
 * Copyright (C) 2003,2004  Jonathan Blandford <jrb@alum.mit.edu>
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
 * Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xdgmime.h"
#include "xdgmimeint.h"
#include "xdgmimeglob.h"
#include "xdgmimemagic.h"
#include "xdgmimealias.h"
#include "xdgmimeparent.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>

typedef struct XdgDirTimeList XdgDirTimeList;
typedef struct XdgCallbackList XdgCallbackList;

static int need_reread = TRUE;
static time_t last_stat_time = 0;

static XdgGlobHash *global_hash = NULL;
static XdgMimeMagic *global_magic = NULL;
static XdgAliasList *alias_list = NULL;
static XdgParentList *parent_list = NULL;
static XdgDirTimeList *dir_time_list = NULL;
static XdgCallbackList *callback_list = NULL;
const char *xdg_mime_type_unknown = "application/octet-stream";


enum
{
    XDG_CHECKED_UNCHECKED,
    XDG_CHECKED_VALID,
    XDG_CHECKED_INVALID
};

struct XdgDirTimeList
{
    time_t mtime;
    char *directory_name;
    int checked;
    XdgDirTimeList *next;
};

struct XdgCallbackList
{
    XdgCallbackList *next;
    XdgCallbackList *prev;
    int              callback_id;
    XdgMimeCallback  callback;
    void            *data;
    XdgMimeDestroy   destroy;
};

/* Function called by xdg_run_command_on_dirs.  If it returns TRUE, further
 * directories aren't looked at */
typedef int (*XdgDirectoryFunc)(const char *directory,
                                void       *user_data);

static XdgDirTimeList *
xdg_dir_time_list_new(void)
{
    XdgDirTimeList *retval;

    retval = (XdgDirTimeList *)calloc(1, sizeof(XdgDirTimeList));
    retval->checked = XDG_CHECKED_UNCHECKED;

    return retval;
}

static void
xdg_dir_time_list_free(XdgDirTimeList *list)
{
    XdgDirTimeList *next;

    while (list)
    {
        next = list->next;
        free(list->directory_name);
        free(list);
        list = next;
    }
}

static int
xdg_mime_init_from_directory(const char *directory)
{
    char *file_name;
    struct stat st;
    XdgDirTimeList *list;

    assert(directory != NULL);

    file_name = (char *)malloc(strlen(directory) + strlen("/mime/globs") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/globs");
    if (stat(file_name, &st) == 0)
    {
        _xdg_mime_glob_read_from_file(global_hash, file_name);

        list = xdg_dir_time_list_new();
        list->directory_name = file_name;
        list->mtime = st.st_mtime;
        list->next = dir_time_list;
        dir_time_list = list;
    }
    else
    {
        free(file_name);
    }

    file_name = (char *)malloc(strlen(directory) + strlen("/mime/magic") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/magic");
    if (stat(file_name, &st) == 0)
    {
        _xdg_mime_magic_read_from_file(global_magic, file_name);

        list = xdg_dir_time_list_new();
        list->directory_name = file_name;
        list->mtime = st.st_mtime;
        list->next = dir_time_list;
        dir_time_list = list;
    }
    else
    {
        free(file_name);
    }

    file_name = (char *)malloc(strlen(directory) + strlen("/mime/aliases") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/aliases");
    _xdg_mime_alias_read_from_file(alias_list, file_name);
    free(file_name);

    file_name = (char *)malloc(strlen(directory) + strlen("/mime/subclasses") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/subclasses");
    _xdg_mime_parent_read_from_file(parent_list, file_name);
    free(file_name);

    return FALSE; /* Keep processing */
}

/* Runs a command on all the directories in the search path */
static void
xdg_run_command_on_dirs(XdgDirectoryFunc  func,
                        void             *user_data)
{
    const char *xdg_data_home;
    const char *xdg_data_dirs;
    const char *ptr;

    xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home)
    {
        if ((func)(xdg_data_home, user_data))
            return;
    }
    else
    {
        const char *home;

        home = getenv("HOME");
        if (home != NULL)
        {
            char *guessed_xdg_home;
            int stop_processing;

            guessed_xdg_home = (char *)malloc(strlen(home) + strlen("/.local/share/") + 1);
            strcpy(guessed_xdg_home, home);
            strcat(guessed_xdg_home, "/.local/share/");
            stop_processing = (func)(guessed_xdg_home, user_data);
            free(guessed_xdg_home);

            if (stop_processing)
                return;
        }
    }

    xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs == NULL)
        xdg_data_dirs = "/usr/local/share/:/usr/share/";

    ptr = xdg_data_dirs;

    while (*ptr != '\000')
    {
        const char *end_ptr;
        char *dir;
        int len;
        int stop_processing;

        end_ptr = ptr;
        while (*end_ptr != ':' && *end_ptr != '\000')
            end_ptr ++;

        if (end_ptr == ptr)
        {
            ptr++;
            continue;
        }

        if (*end_ptr == ':')
            len = end_ptr - ptr;
        else
            len = end_ptr - ptr + 1;
        dir = (char *)malloc(len + 1);
        strncpy(dir, ptr, len);
        dir[len] = '\0';
        stop_processing = (func)(dir, user_data);
        free(dir);

        if (stop_processing)
            return;

        ptr = end_ptr;
    }
}

/* Checks file_path to make sure it has the same mtime as last time it was
 * checked.  If it has a different mtime, or if the file doesn't exist, it
 * returns FALSE.
 *
 * FIXME: This doesn't protect against permission changes.
 */
static int
xdg_check_file(const char *file_path)
{
    struct stat st;

    /* If the file exists */
    if (stat(file_path, &st) == 0)
    {
        XdgDirTimeList *list;

        for (list = dir_time_list; list; list = list->next)
        {
            if (! strcmp(list->directory_name, file_path) &&
                    st.st_mtime == list->mtime)
            {
                if (list->checked == XDG_CHECKED_UNCHECKED)
                    list->checked = XDG_CHECKED_VALID;
                else if (list->checked == XDG_CHECKED_VALID)
                    list->checked = XDG_CHECKED_INVALID;

                return (list->checked != XDG_CHECKED_VALID);
            }
        }
        return TRUE;
    }

    return FALSE;
}

static int
xdg_check_dir(const char *directory,
              int        *invalid_dir_list)
{
    int invalid;
    char *file_name;

    assert(directory != NULL);

    /* Check the globs file */
    file_name = (char *)malloc(strlen(directory) + strlen("/mime/globs") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/globs");
    invalid = xdg_check_file(file_name);
    free(file_name);
    if (invalid)
    {
        *invalid_dir_list = TRUE;
        return TRUE;
    }

    /* Check the magic file */
    file_name = (char *)malloc(strlen(directory) + strlen("/mime/magic") + 1);
    strcpy(file_name, directory);
    strcat(file_name, "/mime/magic");
    invalid = xdg_check_file(file_name);
    free(file_name);
    if (invalid)
    {
        *invalid_dir_list = TRUE;
        return TRUE;
    }

    return FALSE; /* Keep processing */
}

/* Walks through all the mime files stat()ing them to see if they've changed.
 * Returns TRUE if they have. */
static int
xdg_check_dirs(void)
{
    XdgDirTimeList *list;
    int invalid_dir_list = FALSE;

    for (list = dir_time_list; list; list = list->next)
        list->checked = XDG_CHECKED_UNCHECKED;

    xdg_run_command_on_dirs((XdgDirectoryFunc) xdg_check_dir,
                            &invalid_dir_list);

    if (invalid_dir_list)
        return TRUE;

    for (list = dir_time_list; list; list = list->next)
    {
        if (list->checked != XDG_CHECKED_VALID)
            return TRUE;
    }

    return FALSE;
}

/* We want to avoid stat()ing on every single mime call, so we only look for
 * newer files every 5 seconds.  This will return TRUE if we need to reread the
 * mime data from disk.
 */
static int
xdg_check_time_and_dirs(void)
{
    struct timeval tv;
    time_t current_time;
    int retval = FALSE;

    gettimeofday(&tv, NULL);
    current_time = tv.tv_sec;

    if (current_time >= last_stat_time + 5)
    {
        retval = xdg_check_dirs();
        last_stat_time = current_time;
    }

    return retval;
}

/* Called in every public function.  It reloads the hash function if need be.
 */
static void
xdg_mime_init(void)
{
    if (xdg_check_time_and_dirs())
    {
        xdg_mime_shutdown();
    }

    if (need_reread)
    {
        global_hash = _xdg_glob_hash_new();
        global_magic = _xdg_mime_magic_new();
        alias_list = _xdg_mime_alias_list_new();
        parent_list = _xdg_mime_parent_list_new();

        xdg_run_command_on_dirs((XdgDirectoryFunc) xdg_mime_init_from_directory,
                                NULL);

        need_reread = FALSE;
    }
}

const char *
xdg_mime_get_mime_type_for_data(const void *data,
                                size_t      len)
{
    const char *mime_type;

    xdg_mime_init();

    mime_type = _xdg_mime_magic_lookup_data(global_magic, data, len);

    if (mime_type)
        return mime_type;

    return XDG_MIME_TYPE_UNKNOWN;
}

const char *
xdg_mime_get_mime_type_for_file(const char *file_name)
{
    const char *mime_type;
    FILE *file;
    unsigned char *data;
    int max_extent;
    int bytes_read;
    struct stat statbuf;
    const char *base_name;

    if (file_name == NULL)
        return NULL;
    if (! _xdg_utf8_validate(file_name))
        return NULL;

    xdg_mime_init();

    base_name = _xdg_get_base_name(file_name);
    mime_type = xdg_mime_get_mime_type_from_file_name(base_name);

    if (mime_type != XDG_MIME_TYPE_UNKNOWN)
        return mime_type;

    if (stat(file_name, &statbuf) != 0)
        return XDG_MIME_TYPE_UNKNOWN;

    if (!S_ISREG(statbuf.st_mode))
        return XDG_MIME_TYPE_UNKNOWN;

    /* FIXME: Need to make sure that max_extent isn't totally broken.  This could
     * be large and need getting from a stream instead of just reading it all
     * in. */
    max_extent = _xdg_mime_magic_get_buffer_extents(global_magic);
    data = (unsigned char *)malloc(max_extent);
    if (data == NULL)
        return XDG_MIME_TYPE_UNKNOWN;

    /* OK to not use CLO_EXEC here because mimedb is single threaded */
    file = fopen(file_name, "r");
    if (file == NULL)
    {
        free(data);
        return XDG_MIME_TYPE_UNKNOWN;
    }

    bytes_read = fread(data, 1, max_extent, file);
    if (ferror(file))
    {
        free(data);
        fclose(file);
        return XDG_MIME_TYPE_UNKNOWN;
    }

    mime_type = _xdg_mime_magic_lookup_data(global_magic, data, bytes_read);

    free(data);
    fclose(file);

    if (mime_type)
        return mime_type;

    return XDG_MIME_TYPE_UNKNOWN;
}

const char *
xdg_mime_get_mime_type_from_file_name(const char *file_name)
{
    const char *mime_type;

    xdg_mime_init();

    mime_type = _xdg_glob_hash_lookup_file_name(global_hash, file_name);
    if (mime_type)
        return mime_type;
    else
        return XDG_MIME_TYPE_UNKNOWN;
}

int
xdg_mime_is_valid_mime_type(const char *mime_type)
{
    /* FIXME: We should make this a better test
     */
    return _xdg_utf8_validate(mime_type);
}

void
xdg_mime_shutdown(void)
{
    XdgCallbackList *list;

    /* FIXME: Need to make this (and the whole library) thread safe */
    if (dir_time_list)
    {
        xdg_dir_time_list_free(dir_time_list);
        dir_time_list = NULL;
    }

    if (global_hash)
    {
        _xdg_glob_hash_free(global_hash);
        global_hash = NULL;
    }
    if (global_magic)
    {
        _xdg_mime_magic_free(global_magic);
        global_magic = NULL;
    }

    if (alias_list)
    {
        _xdg_mime_alias_list_free(alias_list);
        alias_list = NULL;
    }

    if (parent_list)
    {
        _xdg_mime_parent_list_free(parent_list);
    }


    for (list = callback_list; list; list = list->next)
        (list->callback)(list->data);

    need_reread = TRUE;
}

int
xdg_mime_get_max_buffer_extents(void)
{
    xdg_mime_init();

    return _xdg_mime_magic_get_buffer_extents(global_magic);
}

const char *
xdg_mime_unalias_mime_type(const char *mime_type)
{
    const char *lookup;

    xdg_mime_init();

    if ((lookup = _xdg_mime_alias_list_lookup(alias_list, mime_type)) != NULL)
        return lookup;

    return mime_type;
}

int
xdg_mime_mime_type_equal(const char *mime_a,
                         const char *mime_b)
{
    const char *unalias_a, *unalias_b;

    xdg_mime_init();

    unalias_a = xdg_mime_unalias_mime_type(mime_a);
    unalias_b = xdg_mime_unalias_mime_type(mime_b);

    if (strcmp(unalias_a, unalias_b) == 0)
        return 1;

    return 0;
}

int
xdg_mime_media_type_equal(const char *mime_a,
                          const char *mime_b)
{
    char *sep;

    xdg_mime_init();

    sep = const_cast<char*>(strchr(mime_a, '/'));

    if (sep && strncmp(mime_a, mime_b, sep - mime_a + 1) == 0)
        return 1;

    return 0;
}

#if 0
static int
xdg_mime_is_super_type(const char *mime)
{
    int length;
    const char *type;

    length = strlen(mime);
    type = &(mime[length - 2]);

    if (strcmp(type, "/*") == 0)
        return 1;

    return 0;
}
#endif

int
xdg_mime_mime_type_subclass(const char *mime,
                            const char *base)
{
    const char *umime, *ubase;
    const char **parents;

    xdg_mime_init();

    umime = xdg_mime_unalias_mime_type(mime);
    ubase = xdg_mime_unalias_mime_type(base);

    if (strcmp(umime, ubase) == 0)
        return 1;

#if 0
    /* Handle supertypes */
    if (xdg_mime_is_super_type(ubase) &&
            xdg_mime_media_type_equal(umime, ubase))
        return 1;
#endif

    /*  Handle special cases text/plain and application/octet-stream */
    if (strcmp(ubase, "text/plain") == 0 &&
            strncmp(umime, "text/", 5) == 0)
        return 1;

    if (strcmp(ubase, "application/octet-stream") == 0)
        return 1;

    parents = _xdg_mime_parent_list_lookup(parent_list, umime);
    for (; parents && *parents; parents++)
    {
        if (xdg_mime_mime_type_subclass(*parents, ubase))
            return 1;
    }

    return 0;
}

const char **
xdg_mime_get_mime_parents(const char *mime)
{
    const char *umime;

    xdg_mime_init();

    umime = xdg_mime_unalias_mime_type(mime);

    return _xdg_mime_parent_list_lookup(parent_list, umime);
}

void
xdg_mime_dump(void)
{
    printf("*** ALIASES ***\n\n");
    _xdg_mime_alias_list_dump(alias_list);
    printf("\n*** PARENTS ***\n\n");
    _xdg_mime_parent_list_dump(parent_list);
}


/* Registers a function to be called every time the mime database reloads its files
 */
int
xdg_mime_register_reload_callback(XdgMimeCallback  callback,
                                  void            *data,
                                  XdgMimeDestroy   destroy)
{
    XdgCallbackList *list_el;
    static int callback_id = 1;

    /* Make a new list element */
    list_el = (XdgCallbackList *)calloc(1, sizeof(XdgCallbackList));
    list_el->callback_id = callback_id;
    list_el->callback = callback;
    list_el->data = data;
    list_el->destroy = destroy;
    list_el->next = callback_list;
    if (list_el->next)
        list_el->next->prev = list_el;

    callback_list = list_el;
    callback_id ++;

    return callback_id - 1;
}

void
xdg_mime_remove_callback(int callback_id)
{
    XdgCallbackList *list;

    for (list = callback_list; list; list = list->next)
    {
        if (list->callback_id == callback_id)
        {
            if (list->next)
                list->next = list->prev;

            if (list->prev)
                list->prev->next = list->next;
            else
                callback_list = list->next;

            /* invoke the destroy handler */
            (list->destroy)(list->data);
            free(list);
            return;
        }
    }
}
