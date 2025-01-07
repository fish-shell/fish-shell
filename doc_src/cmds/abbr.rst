.. _cmd-abbr:

abbr - manage fish abbreviations
================================

Synopsis
--------

.. synopsis::

    abbr --add NAME [--position command | anywhere] [-r | --regex PATTERN] [-c | --command COMMAND]
                    [--set-cursor[=MARKER]] ([-f | --function FUNCTION] | EXPANSION)
    abbr --erase NAME ...
    abbr --rename OLD_WORD NEW_WORD
    abbr --show
    abbr --list
    abbr --query NAME ...

Description
-----------

``abbr`` manages abbreviations - user-defined words that are replaced with longer phrases when entered.

.. note::
    Only typed-in commands use abbreviations. Abbreviations are not expanded in scripts.

For example, a frequently-run command like ``git checkout`` can be abbreviated to ``gco``.
After entering ``gco`` and pressing :kbd:`space` or :kbd:`enter`, the full text ``git checkout`` will appear in the command line.
To avoid expanding something that looks like an abbreviation, the default :kbd:`ctrl-space` binding inserts a space without expanding.

An abbreviation may match a literal word, or it may match a pattern given by a regular expression. When an abbreviation matches a word, that word is replaced by new text, called its *expansion*. This expansion may be a fixed new phrase, or it can be dynamically created via a fish function. This expansion occurs after pressing space or enter.

Combining these features, it is possible to create custom syntaxes, where a regular expression recognizes matching tokens, and the expansion function interprets them. See the `Examples`_ section.

.. versionchanged:: 3.6.0
   Previous versions of this allowed saving abbreviations in universal variables.
   That's no longer possible. Existing variables will still be imported and ``abbr --erase`` will also erase the variables.
   We recommend adding abbreviations to :ref:`config.fish <configuration>` by just adding the ``abbr --add`` command.
   When you run ``abbr``, you will see output like this

   ::

     > abbr
     abbr -a -- foo bar # imported from a universal variable, see `help abbr`

   In that case you should take the part before the ``#`` comment and save it in :ref:`config.fish <configuration>`,
   then you can run ``abbr --erase`` to remove the universal variable::

     > abbr >> ~/.config/fish/config.fish
     > abbr --erase (abbr --list)
   
   Alternatively you can keep them in a separate :ref:`configuration file <configuration>` by doing something like the following::

     > abbr > ~/.config/fish/conf.d/myabbrs.fish

   This will save all your abbreviations in "myabbrs.fish", overwriting the whole file so it doesn't leave any duplicates,
   or restore abbreviations you had erased.
   Of course any functions will have to be saved separately, see :doc:`funcsave <funcsave>`.

"add" subcommand
--------------------

.. synopsis::

    abbr [-a | --add] NAME [--position command | anywhere] [-r | --regex PATTERN]
         [-c | --command COMMAND] [--set-cursor[=MARKER]] ([-f | --function FUNCTION] | EXPANSION)

``abbr --add`` creates a new abbreviation. With no other options, the string **NAME** is replaced by **EXPANSION**.

With **--position command**, the abbreviation will only expand when it is positioned as a command, not as an argument to another command. With **--position anywhere** the abbreviation may expand anywhere in the command line. The default is **command**.

With **--command COMMAND**, the abbreviation will only expand when it is used as an argument to the given COMMAND. Multiple **--command** can be used together, and the abbreviation will expand for each. An empty **COMMAND** means it will expand only when there is no command. **--command** implies **--position anywhere** and disallows **--position command**. Even with different **COMMANDS**, the **NAME** of the abbreviation needs to be unique. Consider using **--regex** if you want to expand the same word differently for multiple commands.

With **--regex**, the abbreviation matches using the regular expression given by **PATTERN**, instead of the literal **NAME**. The pattern is interpreted using PCRE2 syntax and must match the entire token. If multiple abbreviations match the same token, the last abbreviation added is used.

With **--set-cursor=MARKER**, the cursor is moved to the first occurrence of **MARKER** in the expansion. The **MARKER** value is erased. The **MARKER** may be omitted (i.e. simply ``--set-cursor``), in which case it defaults to ``%``.

With **-f FUNCTION** or **--function FUNCTION**, **FUNCTION** is treated as the name of a fish function instead of a literal replacement. When the abbreviation matches, the function will be called with the matching token as an argument. If the function's exit status is 0 (success), the token will be replaced by the function's output; otherwise the token will be left unchanged. No **EXPANSION** may be given separately.


Examples
########

::

    abbr --add gco git checkout

Add a new abbreviation where ``gco`` will be replaced with ``git checkout``.

::

    abbr -a --position anywhere -- -C --color

Add a new abbreviation where ``-C`` will be replaced with ``--color``. The ``--`` allows ``-C`` to be treated as the name of the abbreviation, instead of an option.

::

    abbr -a L --position anywhere --set-cursor "% | less"

Add a new abbreviation where ``L`` will be replaced with ``| less``, placing the cursor before the pipe.


::

    function last_history_item
        echo $history[1]
    end
    abbr -a !! --position anywhere --function last_history_item

This first creates a function ``last_history_item`` which outputs the last entered command. It then adds an abbreviation which replaces ``!!`` with the result of calling this function. Taken together, this is similar to the ``!!`` history expansion feature of bash.

::

    function vim_edit
        echo vim $argv
    end
    abbr -a vim_edit_texts --position command --regex ".+\.txt" --function vim_edit

This first creates a function ``vim_edit`` which prepends ``vim`` before its argument. It then adds an abbreviation which matches commands ending in ``.txt``, and replaces the command with the result of calling this function. This allows text files to be "executed" as a command to open them in vim, similar to the "suffix alias" feature in zsh.

::

    abbr 4DIRS --set-cursor=! "$(string join \n -- 'for dir in */' 'cd $dir' '!' 'cd ..' 'end')"

This creates an abbreviation "4DIRS" which expands to a multi-line loop "template." The template enters each directory and then leaves it. The cursor is positioned ready to enter the command to run in each directory, at the location of the ``!``, which is itself erased.

::
   abbr --command git co checkout

Turns "co" as an argument to "git" into "checkout". Multiple commands are possible, ``--command={git,hg}`` would expand "co" to "checkout" for both git and hg.

Other subcommands
--------------------


::

    abbr --rename OLD_NAME NEW_NAME

Renames an abbreviation, from *OLD_NAME* to *NEW_NAME*

::

    abbr [-s | --show]

Show all abbreviations in a manner suitable for import and export

::

    abbr [-l | --list]

Prints the names of all abbreviation

::

    abbr [-e | --erase] NAME

Erases the abbreviation with the given name

::

    abbr -q or --query [NAME...]

Return 0 (true) if one of the *NAME* is an abbreviation.

::

    abbr -h or --help

Displays help for the `abbr` command.

