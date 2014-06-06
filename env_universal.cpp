#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "env_universal_common.h"
#include "env_universal.h"
#include "env.h"


/**
   Set to true after initialization has been performed
*/
static bool s_env_univeral_inited = false;
static void (*external_callback)(fish_message_type_t type, const wchar_t *name, const wchar_t *val);

/**
   Flag set to 1 when a barrier reply is recieved
*/
static int barrier_reply = 0;

void env_universal_barrier();

/**
   Callback function used whenever a new fishd message is recieved
*/
static void callback(fish_message_type_t type, const wchar_t *name, const wchar_t *val)
{
    if (type == BARRIER_REPLY)
    {
        barrier_reply = 1;
    }
    else
    {
        if (external_callback)
            external_callback(type, name, val);
    }
}

void env_universal_init(void (*cb)(fish_message_type_t type, const wchar_t *name, const wchar_t *val))
{
    external_callback = cb;
    env_universal_common_init(&callback);
    s_env_univeral_inited = true;
}

void env_universal_destroy()
{
    s_env_univeral_inited = false;
}


env_var_t env_universal_get(const wcstring &name)
{
    if (!s_env_univeral_inited)
        return env_var_t::missing_var();

    return env_universal_common_get(name);
}

bool env_universal_get_export(const wcstring &name)
{
    if (!s_env_univeral_inited)
        return false;

    return env_universal_common_get_export(name);
}

void env_universal_barrier()
{
    ASSERT_IS_MAIN_THREAD();
    UNIVERSAL_LOG("BARRIER");
    env_universal_common_sync();
}


void env_universal_set(const wcstring &name, const wcstring &value, bool exportv)
{
    if (!s_env_univeral_inited)
        return;

    debug(3, L"env_universal_set( \"%ls\", \"%ls\" )", name.c_str(), value.c_str());
    
    env_universal_common_set(name.c_str(), value.c_str(), exportv);
    env_universal_barrier();
}

int env_universal_remove(const wchar_t *name)
{
    int res;

    if (!s_env_univeral_inited)
        return 1;

    CHECK(name, 1);

    const wcstring name_str = name;
    // TODO: shouldn't have to call get() here, should just have remove return success
    res = env_universal_common_get(name_str).missing();
    env_universal_common_remove(name_str);
    return res;
}

void env_universal_get_names(wcstring_list_t &lst,
                             bool show_exported,
                             bool show_unexported)
{
    if (!s_env_univeral_inited)
        return;

    env_universal_common_get_names(lst,
                                   show_exported,
                                   show_unexported);
}

