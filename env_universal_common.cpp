/**
   \file env_universal_common.c

   The utility library for universal variables. Used both by the
   client library and by the daemon.

*/
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
#include <sys/stat.h>
#include <dirent.h>
#include <wctype.h>

#include <errno.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <map>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "utf8.h"
#include "env_universal_common.h"
#include "path.h"

/**
   Non-wide version of the set command
*/
#define SET_MBS "SET"

/**
   Non-wide version of the set_export command
*/
#define SET_EXPORT_MBS "SET_EXPORT"

/**
   Non-wide version of the erase command
*/
#define ERASE_MBS "ERASE"

/**
   Non-wide version of the barrier command
*/
#define BARRIER_MBS "BARRIER"

/**
   Non-wide version of the barrier_reply command
*/
#define BARRIER_REPLY_MBS "BARRIER_REPLY"

/**
   Error message
*/
#define PARSE_ERR L"Unable to parse universal variable message: '%ls'"

/**
   ERROR string for internal buffered reader
*/
#define ENV_UNIVERSAL_ERROR 0x100

/**
   EAGAIN string for internal buffered reader
*/
#define ENV_UNIVERSAL_AGAIN 0x101

/**
   EOF string for internal buffered reader
*/
#define ENV_UNIVERSAL_EOF   0x102

/**
   The table of all universal variables
*/
static env_universal_t s_env_universal_var;

/**
   Callback function, should be called on all events
*/
static void (*callback)(fish_message_type_t type,
                        const wchar_t *key,
                        const wchar_t *val);

/* UTF <-> wchar conversions. These return a string allocated with malloc. These call sites could be cleaned up substantially to eliminate the dependence on malloc. */
static wchar_t *utf2wcs(const char *input)
{
    wchar_t *result = NULL;
    wcstring converted;
    if (utf8_to_wchar_string(input, &converted))
    {
        result = wcsdup(converted.c_str());
    }
    return result;
}

static char *wcs2utf(const wchar_t *input)
{
    char *result = NULL;
    std::string converted;
    if (wchar_to_utf8_string(input, &converted))
    {
        result = strdup(converted.c_str());
    }
    return result;
}

void env_universal_common_init(void (*cb)(fish_message_type_t type, const wchar_t *key, const wchar_t *val))
{
    callback = cb;
}

void read_message(connection_t *conn)
{
    return s_env_universal_var.read_message(conn);
}

/**
   Read one byte of date form the specified connection
 */
static int read_byte(connection_t *src)
{

    if (src->buffer_consumed >= src->read_buffer.size())
    {
        char local[ENV_UNIVERSAL_BUFFER_SIZE];

        ssize_t res = read(src->fd, local, sizeof local);

//    debug(4, L"Read chunk '%.*s'", res, src->buffer );

        if (res < 0)
        {

            if (errno == EAGAIN ||
                    errno == EINTR)
            {
                return ENV_UNIVERSAL_AGAIN;
            }

            return ENV_UNIVERSAL_ERROR;

        }
        else if (res == 0)
        {
            return ENV_UNIVERSAL_EOF;
        }
        else
        {
            src->read_buffer.clear();
            src->read_buffer.insert(src->read_buffer.begin(), local, local + res);
            src->buffer_consumed = 0;
        }
    }

    return src->read_buffer.at(src->buffer_consumed++);

}

/**
   Remove variable with specified name
*/
void env_universal_common_remove(const wcstring &name)
{
    s_env_universal_var.remove(name);
}

/**
   Test if the message msg contains the command cmd
*/
static bool match(const wchar_t *msg, const wchar_t *cmd)
{
    size_t len = wcslen(cmd);
    if (wcsncasecmp(msg, cmd, len) != 0)
        return false;

    if (msg[len] && msg[len]!= L' ' && msg[len] != L'\t')
        return false;

    return true;
}

void env_universal_common_set(const wchar_t *key, const wchar_t *val, bool exportv)
{
    CHECK(key,);
    CHECK(val,);
    
    s_env_universal_var.set(key, val, exportv);

    if (callback)
    {
        callback(exportv?SET_EXPORT:SET, key, val);
    }
}

/**
   Attempt to send the specified message to the specified file descriptor

   \return 1 on sucess, 0 if the message could not be sent without blocking and -1 on error
*/
static int try_send(message_t *msg,
                    int fd)
{

    debug(3,
          L"before write of %d chars to fd %d", msg->body.size(), fd);

    ssize_t res = write(fd, msg->body.c_str(), msg->body.size());

    if (res != -1)
    {
        debug(4, L"Wrote message '%s'", msg->body.c_str());
    }
    else
    {
        debug(4, L"Failed to write message '%s'", msg->body.c_str());
    }

    if (res == -1)
    {
        switch (errno)
        {
            case EAGAIN:
                return 0;

            default:
                debug(2,
                      L"Error while sending universal variable message to fd %d. Closing connection",
                      fd);
                if (debug_level > 2)
                    wperror(L"write");

                return -1;
        }
    }
    msg->count--;

    if (!msg->count)
    {
        delete msg;
    }
    return 1;
}

void try_send_all(connection_t *c)
{
    /*  debug( 3,
           L"Send all updates to connection on fd %d",
           c->fd );*/
    while (!c->unsent.empty())
    {
        switch (try_send(c->unsent.front(), c->fd))
        {
            case 1:
                c->unsent.pop();
                break;

            case 0:
                debug(4,
                      L"Socket full, send rest later");
                return;

            case -1:
                c->killme = 1;
                return;
        }
    }
}

/* The universal variable format has some funny escaping requirements; here we try to be safe */
static bool is_universal_safe_to_encode_directly(wchar_t c)
{
    if (c < 32 || c > 128)
        return false;

    return iswalnum(c) || wcschr(L"/", c);
}

/**
   Escape specified string
 */
static wcstring full_escape(const wchar_t *in)
{
    wcstring out;
    for (; *in; in++)
    {
        wchar_t c = *in;
        if (is_universal_safe_to_encode_directly(c))
        {
            out.push_back(c);
        }
        else if (c <= ASCII_MAX)
        {
            // See #1225 for discussion of use of ASCII_MAX here
            append_format(out, L"\\x%.2x", c);
        }
        else if (c < 65536)
        {
            append_format(out, L"\\u%.4x", c);
        }
        else
        {
            append_format(out, L"\\U%.8x", c);
        }
    }
    return out;
}

/* Sets the body of a message to the null-terminated list of null terminated const char *. */
void set_body(message_t *msg, ...)
{
    /* Start by counting the length of all the strings */
    size_t body_len = 0;
    const char *arg;
    va_list arg_list;
    va_start(arg_list, msg);
    while ((arg = va_arg(arg_list, const char *)) != NULL)
        body_len += strlen(arg);
    va_end(arg_list);

    /* Reserve that length in the string */
    msg->body.reserve(body_len + 1); //+1 for trailing NULL? Do I need that?

    /* Set the string contents */
    va_start(arg_list, msg);
    while ((arg = va_arg(arg_list, const char *)) != NULL)
        msg->body.append(arg);
    va_end(arg_list);
}

/* Returns an instance of message_t allocated via new */
message_t *create_message(fish_message_type_t type,
                          const wchar_t *key_in,
                          const wchar_t *val_in)
{
    char *key = NULL;

    //  debug( 4, L"Crete message of type %d", type );

    if (key_in)
    {
        if (wcsvarname(key_in))
        {
            debug(0, L"Illegal variable name: '%ls'", key_in);
            return NULL;
        }

        key = wcs2utf(key_in);
        if (!key)
        {
            debug(0,
                  L"Could not convert %ls to narrow character string",
                  key_in);
            return NULL;
        }
    }

    message_t *msg = new message_t;
    msg->count = 0;

    switch (type)
    {
        case SET:
        case SET_EXPORT:
        {
            if (!val_in)
            {
                val_in=L"";
            }

            wcstring esc = full_escape(val_in);
            char *val = wcs2utf(esc.c_str());
            set_body(msg, (type==SET?SET_MBS:SET_EXPORT_MBS), " ", key, ":", val, "\n", NULL);
            free(val);
            break;
        }

        case ERASE:
        {
            set_body(msg, ERASE_MBS, " ", key, "\n", NULL);
            break;
        }

        case BARRIER:
        {
            set_body(msg, BARRIER_MBS, "\n", NULL);
            break;
        }

        case BARRIER_REPLY:
        {
            set_body(msg, BARRIER_REPLY_MBS, "\n", NULL);
            break;
        }

        default:
        {
            debug(0, L"create_message: Unknown message type");
        }
    }

    free(key);

    //  debug( 4, L"Message body is '%s'", msg->body );

    return msg;
}

/**
  Put exported or unexported variables in a string list
*/
void env_universal_common_get_names(wcstring_list_t &lst, bool show_exported, bool show_unexported)
{
    wcstring_list_t names = s_env_universal_var.get_names(show_exported, show_unexported);
    lst.insert(lst.end(), names.begin(), names.end());
}


env_var_t env_universal_common_get(const wcstring &name)
{
    return s_env_universal_var.get(name);
}

bool env_universal_common_get_export(const wcstring &name)
{
    return s_env_universal_var.get_export(name);
}

void enqueue_all(connection_t *c)
{
    s_env_universal_var.enqueue_all(c);
    try_send_all(c);
}

connection_t::connection_t(int input_fd) :
    fd(input_fd),
    killme(false),
    buffer_consumed(0)
{
}

void connection_destroy(connection_t *c)
{
    /*
      A connection need not always be open - we only try to close it
      if it is open.
    */
    if (c->fd >= 0)
    {
        if (close(c->fd))
        {
            wperror(L"close");
        }
    }
}

env_universal_t::env_universal_t() : tried_renaming(false)
{
    VOMIT_ON_FAILURE(pthread_mutex_init(&lock, NULL));
}

env_universal_t::~env_universal_t()
{
    pthread_mutex_destroy(&lock);
}

env_var_t env_universal_t::get(const wcstring &name) const
{
    env_var_t result = env_var_t::missing_var();
    var_table_t::const_iterator where = vars.find(name);
    if (where != vars.end())
    {
        result = where->second.val;
    }
    return result;
}

bool env_universal_t::get_export(const wcstring &name) const
{
    bool result = false;
    var_table_t::const_iterator where = vars.find(name);
    if (where != vars.end())
    {
        result = where->second.exportv;
    }
    return result;
}


void env_universal_t::set_internal(const wcstring &key, const wcstring &val, bool exportv)
{
    ASSERT_IS_LOCKED(lock);
    var_entry_t *entry = &vars[key];
    entry->val = val;
    entry->exportv = exportv;
}

void env_universal_t::set(const wcstring &key, const wcstring &val, bool exportv)
{
    scoped_lock locker(lock);
    this->set_internal(key, val, exportv);
}

void env_universal_t::remove_internal(const wcstring &key)
{
    scoped_lock locker(lock);
    vars.erase(key);
}

void env_universal_t::remove(const wcstring &key)
{
    ASSERT_IS_LOCKED(lock);
    this->remove(key);
}

wcstring_list_t env_universal_t::get_names(bool show_exported, bool show_unexported) const
{
    wcstring_list_t result;
    scoped_lock locker(lock);
    var_table_t::const_iterator iter;
    for (iter = vars.begin(); iter != vars.end(); ++iter)
    {
        const wcstring &key = iter->first;
        const var_entry_t &e = iter->second;
        if ((e.exportv && show_exported) || (! e.exportv && show_unexported))
        {
            result.push_back(key);
        }
    }
    return result;
}

void env_universal_t::enqueue_all(connection_t *c) const
{
    scoped_lock locker(lock);
    var_table_t::const_iterator iter;
    for (iter = vars.begin(); iter != vars.end(); ++iter)
    {
        const wcstring &key = iter->first;
        const var_entry_t &entry = iter->second;
        
        message_t *msg = create_message(entry.exportv ? SET_EXPORT : SET, key.c_str(), entry.val.c_str());
        msg->count=1;
        c->unsent.push(msg);
    }
}

bool env_universal_t::load_from_path(const wcstring &path)
{
    ASSERT_IS_LOCKED(lock);
    /* OK to not use CLO_EXEC here because fishd is single threaded */
    bool result = false;
    int fd = wopen_cloexec(path, O_RDONLY | O_CLOEXEC, 0600);
    if (fd >= 0)
    {
        /* Success */
        result = true;
        connection_t c(fd);
        /* Read from the file */
        this->read_message(&c);
        connection_destroy(&c);
    }
    return result;

}

/**
 Get environment variable value.
 */
static env_var_t fishd_env_get(const char *key)
{
    const char *env = getenv(key);
    if (env != NULL)
    {
        return env_var_t(str2wcstring(env));
    }
    else
    {
        const wcstring wkey = str2wcstring(key);
        return env_universal_common_get(wkey);
    }
}

static wcstring fishd_get_config()
{
    bool done = false;
    wcstring result;
    
    env_var_t xdg_dir = fishd_env_get("XDG_CONFIG_HOME");
    if (! xdg_dir.missing_or_empty())
    {
        result = xdg_dir;
        append_path_component(result, L"/fish");
        if (!create_directory(result))
        {
            done = true;
        }
    }
    else
    {
        env_var_t home = fishd_env_get("HOME");
        if (! home.missing_or_empty())
        {
            result = home;
            append_path_component(result, L"/.config/fish");
            if (!create_directory(result))
            {
                done = 1;
            }
        }
    }
    
    if (! done)
    {
        /* Bad juju */
        debug(0, _(L"Unable to create a configuration directory for fish. Your personal settings will not be saved. Please set the $XDG_CONFIG_HOME variable to a directory where the current user has write access."));
        result.clear();
    }
    
    return result;
}

static std::string get_variables_file_path(const std::string &dir, const std::string &identifier);
bool env_universal_t::load()
{
    scoped_lock locker(lock);
    wcstring wdir = fishd_get_config();
    const std::string dir = wcs2string(wdir);
    if (dir.empty())
        return false;
    
    const std::string machine_id = get_machine_identifier();
    const std::string machine_id_path = get_variables_file_path(dir, machine_id);
    
    const wcstring path = str2wcstring(machine_id_path);
    bool success = load_from_path(path);
    if (! success && ! tried_renaming && errno == ENOENT)
    {
        /* We failed to load, because the file was not found. Older fish used the hostname only. Try *moving* the filename based on the hostname into place; if that succeeds try again. Silently "upgraded." */
        tried_renaming = true;
        std::string hostname_id;
        if (get_hostname_identifier(&hostname_id) && hostname_id != machine_id)
        {
            std::string hostname_path = get_variables_file_path(dir, hostname_id);
            if (0 == rename(hostname_path.c_str(), machine_id_path.c_str()))
            {
                /* We renamed - try again */
                success = load_from_path(path);
            }
        }
    }
    return success;
}

bool env_universal_t::save()
{
    /* Writes variables */
    scoped_lock locker(lock);
    wcstring wdir = fishd_get_config();
    const std::string dir = wcs2string(wdir);
    if (dir.empty())
        return false;
    
    const std::string machine_id = get_machine_identifier();
    const std::string machine_id_path = get_variables_file_path(dir, machine_id);
    
    const wcstring path = str2wcstring(machine_id_path);
    return save_to_path(path);
}

bool env_universal_t::save_to_path(const wcstring &path)
{
    return false;
}

void env_universal_t::read_message(connection_t *src)
{
    scoped_lock locker(lock);
    while (1)
    {
        
        int ib = read_byte(src);
        char b;
        
        switch (ib)
        {
            case ENV_UNIVERSAL_AGAIN:
            {
                return;
            }
                
            case ENV_UNIVERSAL_ERROR:
            {
                debug(2, L"Read error on fd %d, set killme flag", src->fd);
                if (debug_level > 2)
                    wperror(L"read");
                src->killme = 1;
                return;
            }
                
            case ENV_UNIVERSAL_EOF:
            {
                src->killme = 1;
                debug(3, L"Fd %d has reached eof, set killme flag", src->fd);
                if (! src->input.empty())
                {
                    char c = 0;
                    src->input.push_back(c);
                    debug(1,
                          L"Universal variable connection closed while reading command. Partial command recieved: '%s'",
                          &src->input.at(0));
                }
                return;
            }
        }
        
        b = (char)ib;
        
        if (b == '\n')
        {
            wchar_t *msg;
            
            b = 0;
            src->input.push_back(b);
            
            msg = utf2wcs(&src->input.at(0));
            
            /*
             Before calling parse_message, we must empty reset
             everything, since the callback function could
             potentially call read_message.
             */
            src->input.clear();
            
            if (msg)
            {
                this->parse_message_internal(msg, src);
            }
            else
            {
                debug(0, _(L"Could not convert message '%s' to wide character string"), &src->input.at(0));
            }
            
            free(msg);
            
        }
        else
        {
            src->input.push_back(b);
        }
    }
}

/**
 Parse message msg
 */
void env_universal_t::parse_message_internal(wchar_t *msg, connection_t *src)
{
    ASSERT_IS_LOCKED(lock);
    
    //  debug( 3, L"parse_message( %ls );", msg );
    
    if (msg[0] == L'#')
        return;
    
    if (match(msg, SET_STR) || match(msg, SET_EXPORT_STR))
    {
        wchar_t *name, *tmp;
        bool exportv = match(msg, SET_EXPORT_STR);
        
        name = msg+(exportv?wcslen(SET_EXPORT_STR):wcslen(SET_STR));
        while (wcschr(L"\t ", *name))
            name++;
        
        tmp = wcschr(name, L':');
        if (tmp)
        {
            const wcstring key(name, tmp - name);
            
            wcstring val;
            if (unescape_string(tmp + 1, &val, 0))
            {
                this->set_internal(key, val, exportv);
            }
        }
        else
        {
            debug(1, PARSE_ERR, msg);
        }
    }
    else if (match(msg, ERASE_STR))
    {
        wchar_t *name, *tmp;
        
        name = msg+wcslen(ERASE_STR);
        while (wcschr(L"\t ", *name))
            name++;
        
        tmp = name;
        while (iswalnum(*tmp) || *tmp == L'_')
            tmp++;
        
        *tmp = 0;
        
        if (!wcslen(name))
        {
            debug(1, PARSE_ERR, msg);
        }
        
        this->remove_internal(name);
        
#warning We're locked when this is invoked - bad news!
        if (callback)
        {
            callback(ERASE, name, 0);
        }
    }
    else if (match(msg, BARRIER_STR))
    {
        message_t *msg = create_message(BARRIER_REPLY, 0, 0);
        msg->count = 1;
        src->unsent.push(msg);
        try_send_all(src);
    }
    else if (match(msg, BARRIER_REPLY_STR))
    {
        if (callback)
        {
            callback(BARRIER_REPLY, 0, 0);
        }
    }
    else
    {
        debug(1, PARSE_ERR, msg);
    }
}



static std::string get_variables_file_path(const std::string &dir, const std::string &identifier)
{
    std::string name;
    name.append(dir);
    name.append("/");
    name.append("fishd.");
    name.append(identifier);
    return name;
}

/**
 Small not about not editing ~/.fishd manually. Inserted at the top of all .fishd files.
 */
#define SAVE_MSG "# This file is automatically generated by the fish.\n# Do NOT edit it directly, your changes will be overwritten.\n"

static bool load_or_save_variables_at_path(bool save, const std::string &path)
{
    bool result = false;
    
    /* OK to not use CLO_EXEC here because fishd is single threaded */
    int fd = open(path.c_str(), save?(O_CREAT | O_TRUNC | O_WRONLY):O_RDONLY, 0600);
    if (fd >= 0)
    {
        /* Success */
        result = true;
        connection_t c(fd);
        
        if (save)
        {
            /* Save to the file */
            write_loop(c.fd, SAVE_MSG, strlen(SAVE_MSG));
            enqueue_all(&c);
        }
        else
        {
            /* Read from the file */
            read_message(&c);
        }
        
        connection_destroy(&c);
    }
    return result;
}


/**
 Maximum length of hostname. Longer hostnames are truncated
 */
#define HOSTNAME_LEN 32

/* Length of a MAC address */
#define MAC_ADDRESS_MAX_LEN 6


/* Thanks to Jan Brittenson
 http://lists.apple.com/archives/xcode-users/2009/May/msg00062.html
 */
#ifdef SIOCGIFHWADDR

/* Linux */
#include <net/if.h>
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN], const char *interface = "eth0")
{
    bool result = false;
    const int dummy = socket(AF_INET, SOCK_STREAM, 0);
    if (dummy >= 0)
    {
        struct ifreq r;
        strncpy((char *)r.ifr_name, interface, sizeof r.ifr_name - 1);
        r.ifr_name[sizeof r.ifr_name - 1] = 0;
        if (ioctl(dummy, SIOCGIFHWADDR, &r) >= 0)
        {
            memcpy(macaddr, r.ifr_hwaddr.sa_data, MAC_ADDRESS_MAX_LEN);
            result = true;
        }
        close(dummy);
    }
    return result;
}

#elif defined(HAVE_GETIFADDRS)

/* OS X and BSD */
#include <ifaddrs.h>
#include <net/if_dl.h>
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN], const char *interface = "en0")
{
    // BSD, Mac OS X
    struct ifaddrs *ifap;
    bool ok = false;
    
    if (getifaddrs(&ifap) == 0)
    {
        for (const ifaddrs *p = ifap; p; p = p->ifa_next)
        {
            if (p->ifa_addr->sa_family == AF_LINK)
            {
                if (p->ifa_name && p->ifa_name[0] &&
                    ! strcmp((const char*)p->ifa_name, interface))
                {
                    
                    const sockaddr_dl& sdl = *(sockaddr_dl*)p->ifa_addr;
                    
                    size_t alen = sdl.sdl_alen;
                    if (alen > MAC_ADDRESS_MAX_LEN) alen = MAC_ADDRESS_MAX_LEN;
                    memcpy(macaddr, sdl.sdl_data + sdl.sdl_nlen, alen);
                    ok = true;
                    break;
                }
            }
        }
        freeifaddrs(ifap);
    }
    return ok;
}

#else

/* Unsupported */
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN])
{
    return false;
}

#endif

/* Function to get an identifier based on the hostname */
bool get_hostname_identifier(std::string *result)
{
    bool success = false;
    char hostname[HOSTNAME_LEN + 1] = {};
    if (gethostname(hostname, HOSTNAME_LEN) == 0)
    {
        result->assign(hostname);
        success = true;
    }
    return success;
}

/* Get a sort of unique machine identifier. Prefer the MAC address; if that fails, fall back to the hostname; if that fails, pick something. */
std::string get_machine_identifier(void)
{
    std::string result;
    unsigned char mac_addr[MAC_ADDRESS_MAX_LEN] = {};
    if (get_mac_address(mac_addr))
    {
        result.reserve(2 * MAC_ADDRESS_MAX_LEN);
        for (size_t i=0; i < MAC_ADDRESS_MAX_LEN; i++)
        {
            char buff[3];
            snprintf(buff, sizeof buff, "%02x", mac_addr[i]);
            result.append(buff);
        }
    }
    else if (get_hostname_identifier(&result))
    {
        /* Hooray */
    }
    else
    {
        /* Fallback */
        result.assign("nohost");
    }
    return result;
}
