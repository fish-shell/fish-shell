#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
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

/**
   Maximum number of times to try to get a new fishd socket
*/

#define RECONNECT_COUNT 32


connection_t env_universal_server(-1);

/**
   Set to true after initialization has been performed
*/
static bool s_env_univeral_inited = false;

/**
   The number of attempts to start fishd
*/
static int get_socket_count = 0;

#define DEFAULT_RETRY_COUNT 15
#define DEFAULT_RETRY_DELAY 0.2

static wchar_t * path;
static wchar_t *user;
static void (*start_fishd)();
static void (*external_callback)(fish_message_type_t type, const wchar_t *name, const wchar_t *val);

/**
   Flag set to 1 when a barrier reply is recieved
*/
static int barrier_reply = 0;

void env_universal_barrier();

static int is_dead()
{
    return env_universal_server.fd < 0;
}

static int try_get_socket_once(void)
{
    int s;

    wchar_t *wdir;
    wchar_t *wuname;
    char *dir = 0;

    wdir = path;
    wuname = user;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        wperror(L"socket");
        return -1;
    }

    if (wdir)
        dir = wcs2str(wdir);
    else
        dir = strdup("/tmp");

    std::string uname;
    if (wuname)
    {
        uname = wcs2string(wuname);
    }
    else
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_name)
        {
            uname = pw->pw_name;
        }
    }

    std::string name;
    name.reserve(strlen(dir) + uname.size() + strlen(SOCK_FILENAME) + 2);
    name.append(dir);
    name.append("/");
    name.append(SOCK_FILENAME);
    name.append(uname);

    free(dir);

    debug(3, L"Connect to socket %s at fd %d", name.c_str(), s);

    struct sockaddr_un local = {};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, name.c_str(), (sizeof local.sun_path) - 1);

    if (connect(s, (struct sockaddr *)&local, sizeof local) == -1)
    {
        close(s);

        /* If it fails on first try, it's probably no serious error, but fishd hasn't been launched yet.
         This happens (at least) on the first concurrent session. */
        if (get_socket_count > 1)
            wperror(L"connect");

        return -1;
    }

    if ((make_fd_nonblocking(s) != 0) || (fcntl(s, F_SETFD, FD_CLOEXEC) != 0))
    {
        wperror(L"fcntl");
        close(s);

        return -1;
    }

    debug(3, L"Connected to fd %d", s);

    return s;
}

/**
   Get a socket for reading from the server
*/
static int get_socket(void)
{
    get_socket_count++;

    int s = try_get_socket_once();
    if (s < 0)
    {
        if (start_fishd)
        {
            debug(2, L"Could not connect to socket %d, starting fishd", s);

            start_fishd();

            for (size_t i=0; s < 0 && i < DEFAULT_RETRY_COUNT; i++)
            {
                if (i > 0)
                {
                    // Wait before next try
                    usleep((useconds_t)(DEFAULT_RETRY_DELAY * 1E6));
                }
                s = try_get_socket_once();
            }
        }
    }

    if (s < 0)
    {
        debug(1, L"Could not connect to universal variable server, already tried manual restart (or no command supplied). You will not be able to share variable values between fish sessions. Is fish properly installed?");
        return -1;
    }

    return s;
}

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

/**
   Make sure the connection is healthy. If not, close it, and try to
   establish a new connection.
*/
static void check_connection(void)
{
    if (! s_env_univeral_inited)
        return;

    if (env_universal_server.killme)
    {
        debug(3, L"Lost connection to universal variable server.");

        if (close(env_universal_server.fd))
        {
            wperror(L"close");
        }

        env_universal_server.fd = -1;
        env_universal_server.killme=0;
        env_universal_server.input.clear();
        env_universal_read_all();
    }
}

/**
   Remove all universal variables.
*/
static void env_universal_remove_all()
{
    size_t i;

    wcstring_list_t lst;
    env_universal_common_get_names(lst,
                                   1,
                                   1);
    for (i=0; i<lst.size(); i++)
    {
        const wcstring &key = lst.at(i);
        env_universal_common_remove(key);
    }

}


/**
   Try to establish a new connection to fishd. If successfull, end
   with call to env_universal_barrier(), to make sure everything is in
   sync.
*/
static void reconnect()
{
    if (get_socket_count >= RECONNECT_COUNT)
        return;

    debug(3, L"Get new fishd connection");

    s_env_univeral_inited = false;
    env_universal_server.buffer_consumed = 0;
    env_universal_server.read_buffer.clear();
    env_universal_server.fd = get_socket();
    s_env_univeral_inited = true;
    if (env_universal_server.fd >= 0)
    {
        env_universal_remove_all();
        env_universal_barrier();
    }
}


void env_universal_init(wchar_t * p,
                        wchar_t *u,
                        void (*sf)(),
                        void (*cb)(fish_message_type_t type, const wchar_t *name, const wchar_t *val))
{
    path=p;
    user=u;
    start_fishd=sf;
    external_callback = cb;

    env_universal_server.fd = get_socket();
    env_universal_common_init(&callback);
    env_universal_read_all();
    s_env_univeral_inited = true;
    if (env_universal_server.fd >= 0)
    {
        env_universal_barrier();
    }
}

void env_universal_destroy()
{
    /*
      Go into blocking mode and send all data before exiting
    */
    if (env_universal_server.fd >= 0)
    {
        if (fcntl(env_universal_server.fd, F_SETFL, 0) != 0)
        {
            wperror(L"fcntl");
        }
        try_send_all(&env_universal_server);
    }

    connection_destroy(&env_universal_server);
    env_universal_server.fd =-1;
    s_env_univeral_inited = false;
}


/**
   Read all available messages from the server.
*/
int env_universal_read_all()
{
    if (! s_env_univeral_inited)
        return 0;

    if (env_universal_server.fd == -1)
    {
        reconnect();
        if (env_universal_server.fd == -1)
            return 0;
    }

    if (env_universal_server.fd != -1)
    {
        read_message(&env_universal_server);
        check_connection();
        return 1;
    }
    else
    {
        debug(2, L"No connection to universal variable server");
        return 0;
    }
}

const wchar_t *env_universal_get(const wcstring &name)
{
    if (!s_env_univeral_inited)
        return NULL;

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
    message_t *msg;
    fd_set fds;

    if (!s_env_univeral_inited || is_dead())
        return;

    barrier_reply = 0;

    /*
      Create barrier request
    */
    msg= create_message(BARRIER, 0, 0);
    msg->count=1;
    env_universal_server.unsent.push(msg);

    /*
      Wait until barrier request has been sent
    */
    debug(3, L"Create barrier");
    while (1)
    {
        try_send_all(&env_universal_server);
        check_connection();

        if (env_universal_server.unsent.empty())
            break;

        if (env_universal_server.fd == -1)
        {
            reconnect();
            debug(2, L"barrier interrupted, exiting");
            return;
        }

        FD_ZERO(&fds);
        FD_SET(env_universal_server.fd, &fds);
        select(env_universal_server.fd+1, 0, &fds, 0, 0);
    }

    /*
      Wait for barrier reply
    */
    debug(3, L"Sent barrier request");
    while (!barrier_reply)
    {
        if (env_universal_server.fd == -1)
        {
            reconnect();
            debug(2, L"barrier interrupted, exiting (2)");
            return;
        }
        FD_ZERO(&fds);
        FD_SET(env_universal_server.fd, &fds);
        select(env_universal_server.fd+1, &fds, 0, 0, 0);
        env_universal_read_all();
    }
    debug(3, L"End barrier");
}


void env_universal_set(const wcstring &name, const wcstring &value, bool exportv)
{
    if (!s_env_univeral_inited)
        return;

    debug(3, L"env_universal_set( \"%ls\", \"%ls\" )", name.c_str(), value.c_str());

    if (is_dead())
    {
        env_universal_common_set(name.c_str(), value.c_str(), exportv);
    }
    else
    {
        message_t *msg = create_message(exportv?SET_EXPORT:SET,
                                        name.c_str(),
                                        value.c_str());

        if (!msg)
        {
            debug(1, L"Could not create universal variable message");
            return;
        }

        msg->count=1;
        env_universal_server.unsent.push(msg);
        env_universal_barrier();
    }
}

int env_universal_remove(const wchar_t *name)
{
    int res;

    if (!s_env_univeral_inited)
        return 1;

    CHECK(name, 1);

    res = !env_universal_common_get(name);
    debug(3,
          L"env_universal_remove( \"%ls\" )",
          name);

    if (is_dead())
    {
        env_universal_common_remove(wcstring(name));
    }
    else
    {
        message_t *msg = create_message(ERASE, name, 0);
        msg->count=1;
        env_universal_server.unsent.push(msg);
        env_universal_barrier();
    }

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
