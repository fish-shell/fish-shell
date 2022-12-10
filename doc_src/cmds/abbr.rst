.. _cmd-abbr:

abbr - manage fish abbreviations
================================

Synopsis
--------

.. synopsis::

    abbr --add NAME [--position command | anywhere] [-r | --regex PATTERN]
                    [--set-cursor[=MARKER]]
                    [-f | --function] EXPANSION
    abbr --erase NAME ...
    abbr --rename OLD_WORD NEW_WORD
    abbr --show
    abbr --list
    abbr --query NAME ...

Description
-----------

``abbr`` manages abbreviations - user-defined words that are replaced with longer phrases after they are entered.

For example, a frequently-run command like ``git checkout`` can be abbreviated to ``gco``.
After entering ``gco`` and pressing :kbd:`Space` or :kbd:`Enter`, the full text ``git checkout`` will appear in the command line.

An abbreviation may match a literal word, or it may match a pattern given by a regular expression. When an abbreviation matches a word, that word is replaced by new text, called its *expansion*. This expansion may be a fixed new phrase, or it can be dynamically created via a fish function. This expansion occurs after pressing space or enter.

Combining these features, it is possible to create custom syntaxes, where a regular expression recognizes matching tokens, and the expansion function interprets them. See the `Examples`_ section.

Abbreviations may be added to :ref:`config.fish <configuration>`. Abbreviations are only expanded for typed-in commands, not in scripts.


"add" subcommand
--------------------

.. synopsis::

    abbr [-a | --add] NAME [--position command | anywhere] [-r | --regex PATTERN]
         [--set-cursor[=MARKER]] [-f | --function] EXPANSION

``abbr --add`` creates a new abbreviation. With no other options, the string **NAME** is replaced by **EXPANSION**.

With **--position command**, the abbreviation will only expand when it is positioned as a command, not as an argument to another command. With **--position anywhere** the abbreviation may expand anywhere in the command line. The default is **command**.

With **--regex**, the abbreviation matches using the regular expression given by **PATTERN**, instead of the literal **NAME**. The pattern is interpreted using PCRE2 syntax and must match the entire token. If multiple abbreviations match the same token, the last abbreviation added is used.

With **--set-cursor=MARKER**, the cursor is moved to the first occurrence of **MARKER** in the expansion. The **MARKER** value is erased. The **MARKER** may be omitted (i.e. simply ``--set-cursor``), in which case it defaults to ``%``.

With **-f** or **--function**, **EXPANSION** is treated as the name of a fish function instead of a literal replacement. When the abbreviation matches, the function will be called with the matching token as an argument. If the function's exit status is 0 (success), the token will be replaced by the function's output; otherwise the token will be left unchanged.


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

    abbr 4DIRS --set-cursor ! "$(string join \n -- 'for dir in */' 'cd $dir' '!' 'cd ..' 'end')"

This creates an abbreviation "4DIRS" which expands to a multi-line loop "template." The template enters each directory and then leaves it. The cursor is positioned ready to enter the command to run in each directory, at the location of the ``!``, which is itself erased.

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

