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
#include <iconv.h>

#include <errno.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <map>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "env_universal_common.h"

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
   A variable entry. Stores the value of a variable and whether it
   should be exported. Obviously, it needs to be allocated large
   enough to fit the value string.
*/
typedef struct var_uni_entry
{
    bool exportv; /**< Whether the variable should be exported */
    wcstring val; /**< The value of the variable */
    var_uni_entry():exportv(false), val() { }
}
var_uni_entry_t;


static void parse_message(wchar_t *msg,
                          connection_t *src);

/**
   The table of all universal variables
*/
typedef std::map<wcstring, var_uni_entry_t> env_var_table_t;
env_var_table_t env_universal_var;

/**
   Callback function, should be called on all events
*/
static void (*callback)(fish_message_type_t type,
                        const wchar_t *key,
                        const wchar_t *val);

/**
   List of names for the UTF-8 character set.
 */
static const char *iconv_utf8_names[]=
{
    "utf-8", "UTF-8",
    "utf8", "UTF8",
    0
}
;

/**
    List of wide character names, undefined byte length.
 */
static const char *iconv_wide_names_unknown[]=
{
    "wchar_t", "WCHAR_T",
    "wchar", "WCHAR",
    0
}
;

/**
   List of wide character names, 4 bytes long.
 */
static const char *iconv_wide_names_4[]=
{
    "wchar_t", "WCHAR_T",
    "wchar", "WCHAR",
    "ucs-4", "UCS-4",
    "ucs4", "UCS4",
    "utf-32", "UTF-32",
    "utf32", "UTF32",
    0
}
;

/**
   List of wide character names, 2 bytes long.
 */
static const char *iconv_wide_names_2[]=
{
    "wchar_t", "WCHAR_T",
    "wchar", "WCHAR",
    "ucs-2", "UCS-2",
    "ucs2", "UCS2",
    "utf-16", "UTF-16",
    "utf16", "UTF16",
    0
}
;

template<class T>
class sloppy {};

static size_t hack_iconv(iconv_t cd, const char * const* inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
    /* FreeBSD has this prototype: size_t iconv (iconv_t, const char **...)
       OS X and Linux this one: size_t iconv (iconv_t, char **...)
       AFAIK there's no single type that can be passed as both char ** and const char **.
       Therefore, we let C++ figure it out, by providing a struct with an implicit conversion to both char** and const char **.
    */
    struct sloppy_char
    {
        const char * const * t;
        operator char** () const
        {
            return (char **)t;
        }
        operator const char** () const
        {
            return (const char**)t;
        }
    } slop_inbuf = {inbuf};

    return iconv(cd, slop_inbuf, inbytesleft, outbuf, outbytesleft);
}

/**
   Convert utf-8 string to wide string
 */
static wchar_t *utf2wcs(const char *in)
{
    iconv_t cd=(iconv_t) -1;
    int i,j;

    wchar_t *out;

    /*
      Try to convert to wchar_t. If that is not a valid character set,
      try various names for ucs-4. We can't be sure that ucs-4 is
      really the character set used by wchar_t, but it is the best
      assumption we can make.
    */
    const char **to_name=0;

    switch (sizeof(wchar_t))
    {

        case 2:
            to_name = iconv_wide_names_2;
            break;

        case 4:
            to_name = iconv_wide_names_4;
            break;

        default:
            to_name = iconv_wide_names_unknown;
            break;
    }


    /*
      The line protocol fish uses is always utf-8.
    */
    const char **from_name = iconv_utf8_names;

    size_t in_len = strlen(in);
    size_t out_len =  sizeof(wchar_t)*(in_len+2);
    size_t nconv;
    char *nout;

    out = (wchar_t *)malloc(out_len);
    nout = (char *)out;

    if (!out)
        return 0;

    for (i=0; to_name[i]; i++)
    {
        for (j=0; from_name[j]; j++)
        {
            cd = iconv_open(to_name[i], from_name[j]);

            if (cd != (iconv_t) -1)
            {
                goto start_conversion;

            }
        }
    }

start_conversion:

    if (cd == (iconv_t) -1)
    {
        /* Something went wrong.  */
        debug(0, L"Could not perform utf-8 conversion");
        if (errno != EINVAL)
            wperror(L"iconv_open");

        /* Terminate the output string.  */
        free(out);
        return 0;
    }

    /* FreeBSD has this prototype: size_t iconv (iconv_t, const char **...)
       OS X and Linux this one: size_t iconv (iconv_t, char **...)
       AFAIK there's no single type that can be passed as both char ** and const char **.
       Hence this hack.
    */
    nconv = hack_iconv(cd, &in, &in_len, &nout, &out_len);

    if (nconv == (size_t) -1)
    {
        debug(0, L"Error while converting from utf string");
        return 0;
    }

    *((wchar_t *) nout) = L'\0';

    /*
      Check for silly iconv behaviour inserting an bytemark in the output
      string.
     */
    if (*out == L'\xfeff' || *out == L'\xffef' || *out == L'\xefbbbf')
    {
        wchar_t *out_old = out;
        out = wcsdup(out+1);
        if (! out)
        {
            debug(0, L"FNORD!!!!");
            free(out_old);
            return 0;
        }
        free(out_old);
    }


    if (iconv_close(cd) != 0)
        wperror(L"iconv_close");

    return out;
}



/**
   Convert wide string to utf-8
 */
static char *wcs2utf(const wchar_t *in)
{
    iconv_t cd=(iconv_t) -1;
    int i,j;

    char *char_in = (char *)in;
    char *out;

    /*
      Try to convert to wchar_t. If that is not a valid character set,
      try various names for ucs-4. We can't be sure that ucs-4 is
      really the character set used by wchar_t, but it is the best
      assumption we can make.
    */
    const char **from_name=0;

    switch (sizeof(wchar_t))
    {

        case 2:
            from_name = iconv_wide_names_2;
            break;

        case 4:
            from_name = iconv_wide_names_4;
            break;

        default:
            from_name = iconv_wide_names_unknown;
            break;
    }

    const char **to_name = iconv_utf8_names;

    size_t in_len = wcslen(in);
    size_t out_len =  sizeof(char)*((MAX_UTF8_BYTES*in_len)+1);
    size_t nconv;
    char *nout;

    out = (char *)malloc(out_len);
    nout = (char *)out;
    in_len *= sizeof(wchar_t);

    if (!out)
        return 0;

    for (i=0; to_name[i]; i++)
    {
        for (j=0; from_name[j]; j++)
        {
            cd = iconv_open(to_name[i], from_name[j]);

            if (cd != (iconv_t) -1)
            {
                goto start_conversion;

            }
        }
    }

start_conversion:

    if (cd == (iconv_t) -1)
    {
        /* Something went wrong.  */
        debug(0, L"Could not perform utf-8 conversion");
        if (errno != EINVAL)
            wperror(L"iconv_open");

        /* Terminate the output string.  */
        free(out);
        return 0;
    }

    nconv = hack_iconv(cd, &char_in, &in_len, &nout, &out_len);


    if (nconv == (size_t) -1)
    {
        debug(0, L"%d %d", in_len, out_len);
        debug(0, L"Error while converting from to string");

        /* Terminate the output string.  */
        free(out);
        return 0;
    }

    *nout = '\0';

    if (iconv_close(cd) != 0)
        wperror(L"iconv_close");

    return out;
}



void env_universal_common_init(void (*cb)(fish_message_type_t type, const wchar_t *key, const wchar_t *val))
{
    callback = cb;
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


void read_message(connection_t *src)
{
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
                parse_message(msg, src);
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
   Remove variable with specified name
*/
void env_universal_common_remove(const wcstring &name)
{
    env_universal_var.erase(name);
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

    var_uni_entry_t &entry = env_universal_var[key];
    entry.exportv=exportv;
    entry.val = val;

    if (callback)
    {
        callback(exportv?SET_EXPORT:SET, key, val);
    }
}


/**
   Parse message msg
*/
static void parse_message(wchar_t *msg,
                          connection_t *src)
{
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
            wchar_t *val;
            const wcstring key(name, tmp - name);

            val = tmp+1;
            val = unescape(val, 0);

            if (val != NULL)
                env_universal_common_set(key.c_str(), val, exportv);

            free(val);
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

        env_universal_common_remove(name);

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
        else if (c < 256)
        {
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
void env_universal_common_get_names(wcstring_list_t &lst,
                                    bool show_exported,
                                    bool show_unexported)
{
    env_var_table_t::const_iterator iter;
    for (iter = env_universal_var.begin(); iter != env_universal_var.end(); ++iter)
    {
        const wcstring &key = iter->first;
        const var_uni_entry_t &e = iter->second;
        if ((e.exportv && show_exported) || (! e.exportv && show_unexported))
        {
            lst.push_back(key);
        }
    }

}


const wchar_t *env_universal_common_get(const wcstring &name)
{
    env_var_table_t::const_iterator result = env_universal_var.find(name);
    if (result != env_universal_var.end())
    {
        const var_uni_entry_t &e = result->second;
        return const_cast<wchar_t*>(e.val.c_str());
    }

    return NULL;
}

bool env_universal_common_get_export(const wcstring &name)
{
    env_var_table_t::const_iterator result = env_universal_var.find(name);
    if (result != env_universal_var.end())
    {
        const var_uni_entry_t &e = result->second;
        return e.exportv;
    }
    return false;
}

void enqueue_all(connection_t *c)
{
    env_var_table_t::const_iterator iter;
    for (iter = env_universal_var.begin(); iter != env_universal_var.end(); ++iter)
    {
        const wcstring &key = iter->first;
        const var_uni_entry_t &entry = iter->second;

        message_t *msg = create_message(entry.exportv ? SET_EXPORT : SET, key.c_str(), entry.val.c_str());
        msg->count=1;
        c->unsent.push(msg);
    }

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
