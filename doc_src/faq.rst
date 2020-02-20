Frequently asked questions
==========================

How do I set or clear an environment variable?
----------------------------------------------
Use the :ref:`set <cmd-set>` command::

    set -x key value
    set -e key

Since fish 3.1 you can set an environment variable for just one command using the ``key=value some command`` syntax, like in other shells.  The two lines below behave identically - unlike other shells, fish will output ``value`` both times::

    key=value echo $key
    begin; set -lx key value; echo $key; end

How do I run a command every login? What's fish's equivalent to .bashrc or .profile?
------------------------------------------------------------------------------------
Edit the file ``~/.config/fish/config.fish``, creating it if it does not exist (Note the leading period).


How do I set my prompt?
-----------------------
The prompt is the output of the ``fish_prompt`` function. Put it in ``~/.config/fish/functions/fish_prompt.fish``. For example, a simple prompt is::

    function fish_prompt
        set_color $fish_color_cwd
        echo -n (prompt_pwd)
        set_color normal
        echo -n ' > '
    end


You can also use the Web configuration tool, :ref:`fish_config <cmd-fish_config>`, to preview and choose from a gallery of sample prompts.


How do I run a command from history?
------------------------------------
Type some part of the command, and then hit the :kbd:`↑` (up) or :kbd:`↓` (down) arrow keys to navigate through history matches. Additional default key bindings include :kbd:`Control+P` (up) and :kbd:`Control+N` (down).


How do I run a subcommand? The backtick doesn't work!
-----------------------------------------------------
``fish`` uses parentheses for subcommands. For example::

    for i in (ls)
        echo $i
    end


My command (pkg-config) gives its output as a single long string?
-----------------------------------------------------------------
Unlike other shells, fish splits command substitutions only on newlines, not spaces or tabs or the characters in $IFS.

That means if you run

::

    echo x(printf '%s ' a b c)x


It will print ``xa b c x``, because the "a b c " is used in one piece. But if you do

::

    echo x(printf '%s\n' a b c)x


it will print ``xax xbx xcx``.

In the overwhelming majority of cases, splitting on spaces is unwanted, so this is an improvement.

However sometimes, especially with ``pkg-config`` and related tools, splitting on spaces is needed.

In these cases use ``string split " "`` like::

    g++ example_01.cpp (pkg-config --cflags --libs gtk+-2.0 | string split " ")


How do I get the exit status of a command?
------------------------------------------
Use the ``$status`` variable. This replaces the ``$?`` variable used in some other shells.

::

    somecommand
    if test $status -eq 7
        echo "That's my lucky number!"
    end


If you are just interested in success or failure, you can run the command directly as the if-condition::

    if somecommand
        echo "Command succeeded"
    else
        echo "Command failed"
    end


Or if you just want to do one command in case the first succeeded or failed, use ``and`` or ``or``::

    somecommand
    or someothercommand

See the documentation for :ref:`test <cmd-test>` and :ref:`if <cmd-if>` for more information.

How do I check whether a variable is defined?
---------------------------------------------

Use ``set -q var``.  For example, ``if set -q var; echo variable defined; end``.  To check multiple variables you can combine with ``and`` and ``or`` like so::

    if set -q var1; or set -q var2
        echo either variable defined
    end

Keep in mind that a defined variabled could also be empty, either by having no elements (if set like ``set var``) or only empty elements (if set like ``set var ""``). Read on for how to deal with those.


How do I check whether a variable is not empty?
-----------------------------------------------

Use ``string length -q -- $var``.  For example, ``if string length -q -- $var; echo not empty; end``.  Note that ``string length`` will interpret a list of multiple variables as a disjunction (meaning any/or)::

    if string length -q -- $var1 $var2 $var3
        echo at least one of these variables is not empty
    end

Alternatively, use ``test -n "$var"``, but remember that **the variable must be double-quoted**.  For example, ``if test -n "$var"; echo not empty; end``. The ``test`` command provides its own and (-a) and or (-o)::

    if test -n "$var1" -o -n "$var2" -o -n "$var3"
        echo at least one of these variables is not empty
    end


If you want to know if a variable has *no elements*, use ``set -q var[1]``.


Why doesn't ``set -Ux`` (exported universal variables) seem to work?
--------------------------------------------------------------------
A global variable of the same name already exists.

Environment variables such as ``EDITOR`` or ``TZ`` can be set universally using ``set -Ux``.  However, if
there is an environment variable already set before fish starts (such as by login scripts or system
administrators), it is imported into fish as a global variable. The :ref:`variable scopes <variables-scope>` are searched from the "inside out", which
means that local variables are checked first, followed by global variables, and finally universal
variables.

This means that the global value takes precedence over the universal value.

To avoid this problem, consider changing the setting which fish inherits. If this is not possible,
add a statement to your :ref:`user initialization file <initialization>` (usually
``~/.config/fish/config.fish``)::

    set -gx EDITOR vim


How do I customize my syntax highlighting colors?
-------------------------------------------------
Use the web configuration tool, :ref:`fish_config <cmd-fish_config>`, or alter the `fish_color family of environment variables <index#variables-color>`__.

How do I update man page completions?
-------------------------------------
Use the :ref:`fish_update_completions <cmd-fish_update_completions>` command.

I accidentally entered a directory path and fish changed directory. What happened?
----------------------------------------------------------------------------------
If fish is unable to locate a command with a given name, and it starts with '``.``', '``/``' or '``~``', fish will test if a directory of that name exists. If it does, it is implicitly assumed that you want to change working directory. For example, the fastest way to switch to your home directory is to simply press ``~`` and enter.


The open command doesn't work.
------------------------------
The ``open`` command uses the MIME type database and the ``.desktop`` files used by Gnome and KDE to identify filetypes and default actions. If at least one of these environments is installed, but the open command is not working, this probably means that the relevant files are installed in a non-standard location. Consider `asking for more help <index#more-help>`__.


How do I make fish my default shell?
------------------------------------
If you installed fish manually (e.g. by compiling it, not by using a package manager), you first need to add fish to the list of shells by executing the following command (assuming you installed fish in /usr/local)::

    echo /usr/local/bin/fish | sudo tee -a /etc/shells


If you installed a prepackaged version of fish, the package manager should have already done this for you.

In order to change your default shell, type::

    chsh -s /usr/local/bin/fish


You may need to adjust the above path to e.g. ``/usr/bin/fish``. Use the command ``which fish`` if you are unsure of where fish is installed.

Unfortunately, there is no way to make the changes take effect at once. You will need to log out and back in again.


I'm seeing weird output before each prompt when using screen. What's wrong?
---------------------------------------------------------------------------
Quick answer:

Run the following command in fish::

    function fish_title; end; funcsave fish_title


Problem solved!

The long answer:

Fish is trying to set the titlebar message of your terminal. While screen itself supports this feature, your terminal does not. Unfortunately, when the underlying terminal doesn't support setting the titlebar, screen simply passes through the escape codes and text to the underlying terminal instead of ignoring them. It is impossible to detect and resolve this problem from inside fish since fish has no way of knowing what the underlying terminal type is. For now, the only way to fix this is to unset the titlebar message, as suggested above.

Note that fish has a default titlebar message, which will be used if the fish_title function is undefined. So simply unsetting the fish_title function will not work.


How do I change the greeting message?
-------------------------------------
Change the value of the variable ``fish_greeting`` or create a ``fish_greeting`` function. For example, to remove the greeting use::

    set fish_greeting



Why doesn't history substitution ("!$" etc.) work?
--------------------------------------------------
Because history substitution is an awkward interface that was invented before interactive line editing was even possible.  Fish drops it in favor of perfecting the interactive history recall interface.  Switching requires a small change of habits: if you want to modify an old line/word, first recall it, then edit.  E.g. don't type "sudo !!" - first press Up, then Home, then type "sudo ".

Fish history recall is very simple yet effective:

- As in any modern shell, the Up arrow, :kbd:`↑` recalls whole lines, starting from the last line executed.  A single press replaces "!!", later presses replace "!-3" and the like.

  - If the line you want is far back in the history, type any part of the line and then press Up one or more times.  This will constrain the recall to lines that include this text, and you will get to the line you want much faster.  This replaces "!vi", "!?bar.c" and the like.

- :kbd:`Alt+↑,Up` recalls individual arguments, starting from the last argument in the last line executed.  A single press replaces "!$", later presses replace "!!:4" and the like. An alternate key binding is :kbd:`Alt+.`.

  - If the argument you want is far back in history (e.g. 2 lines back - that's a lot of words!), type any part of it and then press :kbd:`Alt+↑,Up`.  This will show only arguments containing that part and you will get what you want much faster.  Try it out, this is very convenient!

  - If you want to reuse several arguments from the same line ("!!:3*" and the like), consider recalling the whole line and removing what you don't need (:kbd:`Alt+D` and :kbd:`Alt+Backspace` are your friends).

See :ref:`documentation <editor>` for more details about line editing in fish.


How can I use ``-`` as a shortcut for ``cd -``?
-----------------------------------------------
In fish versions prior to 2.5.0 it was possible to create a function named ``-`` that would do ``cd -``. Changes in the 2.5.0 release included several bug fixes that enforce the rule that a bare hyphen is not a valid function (or variable) name. However, you can achieve the same effect via an abbreviation::

    abbr -a -- - 'cd -'

.. _faq-uninstalling:

Uninstalling fish
-----------------
Should you wish to uninstall fish, first ensure fish is not set as your shell. Run ``chsh -s /bin/bash`` if you are not sure.

Next, do the following (assuming fish was installed to /usr/local)::

    rm -Rf /usr/local/etc/fish /usr/local/share/fish ~/.config/fish
    rm /usr/local/share/man/man1/fish*.1
    cd /usr/local/bin
    rm -f fish fish_indent



Unicode private-use characters reserved by fish
-----------------------------------------------
Fish reserves the `Unicode private-use character range <http://www.unicode.org/faq/private_use.html>`__ from U+F600 thru U+F73F for internal use. Any attempt to feed characters in that range to fish will result in them being replaced by the Unicode "replacement character" U+FFFD. This includes both interactive input as well as any file read by fish (but not programs run by fish).


Where can I find extra tools for fish?
--------------------------------------
The fish user community extends fish in unique and useful ways via scripts that aren't always appropriate for bundling with the fish package. Typically because they solve a niche problem unlikely to appeal to a broad audience. You can find those extensions, including prompts, themes and useful functions, in various third-party repositories. These include:

- `Fisher <https://github.com/jorgebucaran/fisher>`_
- `Fundle <https://github.com/tuvistavie/fundle>`_
- `Oh My Fish <https://github.com/oh-my-fish/oh-my-fish>`_
- `Tacklebox <https://github.com/justinmayer/tacklebox>`_

This is not an exhaustive list and the fish project has no opinion regarding the merits of the repositories listed above or the scripts found therein.
