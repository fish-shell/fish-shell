/** \file kill.c
  The killring.

  Works like the killring in emacs and readline. The killring is cut
  and paste with a memory of previous cuts. It supports integration
  with the X clipboard.
*/
#include <stddef.h>
#include <algorithm>
#include <list>
#include <string>
#include <memory>
#include <stdbool.h>

#include "fallback.h" // IWYU pragma: keep
#include "kill.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "path.h"

/** Kill ring */
typedef std::list<wcstring> kill_list_t;
static kill_list_t kill_list;

/**
   Contents of the X clipboard, at last time we checked it
*/
static wcstring cut_buffer;

/**
   Test if the xsel command is installed.  Since this is called often,
   cache the result.
*/
static int has_xsel()
{
    static signed char res=-1;
    if (res < 0)
    {
        res = !! path_get_path(L"xsel", NULL);
    }

    return res;
}

void kill_add(const wcstring &str)
{
    ASSERT_IS_MAIN_THREAD();
    if (str.empty())
        return;

    wcstring cmd;
    wcstring escaped_str;
    kill_list.push_front(str);

    /*
       Check to see if user has set the FISH_CLIPBOARD_CMD variable,
       and, if so, use it instead of checking the display, etc.

       I couldn't think of a safe way to allow overide of the echo
       command too, so, the command used must accept the input via stdin.
    */

    const env_var_t clipboard_wstr = env_get_string(L"FISH_CLIPBOARD_CMD");
    if (!clipboard_wstr.missing())
    {
        escaped_str = escape(str.c_str(), ESCAPE_ALL);
        cmd.assign(L"echo -n ");
        cmd.append(escaped_str);
        cmd.append(clipboard_wstr);
    }
    else
    {
        /* This is for sending the kill to the X copy-and-paste buffer */
        if (!has_xsel())
        {
            return;
        }

        const env_var_t disp_wstr = env_get_string(L"DISPLAY");
        if (!disp_wstr.missing())
        {
            escaped_str = escape(str.c_str(), ESCAPE_ALL);
            cmd.assign(L"echo -n ");
            cmd.append(escaped_str);
            cmd.append(L" | xsel -i -b");
        }
    }

    if (! cmd.empty())
    {
        if (exec_subshell(cmd, false /* do not apply exit status */) == -1)
        {
            /*
               Do nothing on failiure
            */
        }

        cut_buffer = escaped_str;
    }
}

/**
   Remove first match for specified string from circular list
*/
static void kill_remove(const wcstring &s)
{
    ASSERT_IS_MAIN_THREAD();
    kill_list_t::iterator iter = std::find(kill_list.begin(), kill_list.end(), s);
    if (iter != kill_list.end())
        kill_list.erase(iter);
}



void kill_replace(const wcstring &old, const wcstring &newv)
{
    kill_remove(old);
    kill_add(newv);
}

const wchar_t *kill_yank_rotate()
{
    ASSERT_IS_MAIN_THREAD();
    // Move the first element to the end
    if (kill_list.empty())
    {
        return NULL;
    }
    else
    {
        kill_list.splice(kill_list.end(), kill_list, kill_list.begin());
        return kill_list.front().c_str();
    }
}

/**
   Check the X clipboard. If it has been changed, add the new
   clipboard contents to the fish killring.
*/
static void kill_check_x_buffer()
{
    if (!has_xsel())
        return;

    const env_var_t disp = env_get_string(L"DISPLAY");
    if (! disp.missing())
    {
        size_t i;
        wcstring cmd = L"xsel -t 500 -b";
        wcstring new_cut_buffer=L"";
        wcstring_list_t list;
        if (exec_subshell(cmd, list, false /* do not apply exit status */) != -1)
        {

            for (i=0; i<list.size(); i++)
            {
                wcstring next_line = escape_string(list.at(i), 0);
                if (i > 0) new_cut_buffer += L"\\n";
                new_cut_buffer += next_line;
            }

            if (new_cut_buffer.size() > 0)
            {
                /*
                  The buffer is inserted with backslash escapes,
                  since we don't really like tabs, newlines,
                  etc. anyway.
                */

                if (cut_buffer != new_cut_buffer)
                {
                    cut_buffer = new_cut_buffer;
                    kill_list.push_front(new_cut_buffer);
                }
            }
        }
    }
}


const wchar_t *kill_yank()
{
    kill_check_x_buffer();
    if (kill_list.empty())
    {
        return L"";
    }
    else
    {
        return kill_list.front().c_str();
    }
}

void kill_sanity_check()
{
}

void kill_init()
{
}

void kill_destroy()
{
    cut_buffer.clear();
}

