complete - edit command-specific tab-completions
================================================

Synopsis
--------

.. synopsis::

    complete ((-c | --command) | (-p | --path)) COMMAND [OPTIONS]
    complete (-C | --do-complete) [--escape] STRING

Description
-----------

``complete`` defines, removes or lists completions for a command.

For an introduction to writing your own completions, see :doc:`Writing your own completions <../completions>` in
the fish manual.

The following options are available:

**-c** or **--command** *COMMAND*
    Specifies that *COMMAND* is the name of the command. If there is no **-c** or **-p**, one non-option argument will be used as the command.

**-p** or **--path** *COMMAND*
    Specifies that *COMMAND* is the absolute path of the command (optionally containing wildcards).

**-e** or **--erase**
    Deletes the specified completion.

**-s** or **--short-option** *SHORT_OPTION*
    Adds a short option to the completions list.

**-l** or **--long-option** *LONG_OPTION*
    Adds a GNU-style long option to the completions list.

**-o** or **--old-option** *OPTION*
    Adds an old-style short or long option (see below for details).

**-a** or **--arguments** *ARGUMENTS*
    Adds the specified option arguments to the completions list.

**-k** or **--keep-order**
    Keeps the order of *ARGUMENTS* instead of sorting alphabetically. Multiple ``complete`` calls with **-k** result in arguments of the later ones displayed first.

**-f** or **--no-files**
    This completion may not be followed by a filename.

**-F** or **--force-files**
    This completion may be followed by a filename, even if another applicable ``complete`` specified **--no-files**.

**-r** or **--require-parameter**
    This completion must have an option argument, i.e. may not be followed by another option.
    This means that the next argument is the argument to the option.
    If this is *not* given, the option argument must be attached like ``-xFoo`` or ``--color=auto``.

**-x** or **--exclusive**
    Short for **-r** and **-f**.

**-d** or **--description** *DESCRIPTION*
    Add a description for this completion, to be shown in the completion pager.

**-w** or **--wraps** *WRAPPED_COMMAND*
    Causes the specified command to inherit completions from *WRAPPED_COMMAND*.
    This is used for "this command completes like that other command" kinds of relationships.
    See below for details.

**-n** or **--condition** *CONDITION*
    This completion should only be used if the *CONDITION* (a shell command) returns 0. This makes it possible to specify completions that should only be used in some cases. If multiple conditions are specified, fish will try them in the order they are specified until one fails or all succeeded.

**-C** or **--do-complete** *STRING*
    Makes ``complete`` try to find all possible completions for the specified string. If there is no *STRING*, the current commandline is used instead.

**--escape**
    When used with ``-C``, escape special characters in completions.

**-h** or **--help**
    Displays help about using this command.

Command-specific tab-completions in ``fish`` are based on the notion of options and arguments. An option is a parameter which begins with a hyphen, such as ``-h``, ``-help`` or ``--help``. Arguments are parameters that do not begin with a hyphen. Fish recognizes three styles of options, the same styles as the GNU getopt library. These styles are:

- Short options, like ``-a``. Short options are a single character long, are preceded by a single hyphen and can be grouped together (like ``-la``, which is equivalent to ``-l -a``). Option arguments may be specified by appending the option with the value (``-w32``), or, if ``--require-parameter`` is given, in the following parameter (``-w 32``).

- Old-style options, long like ``-Wall`` or ``-name`` or even short like ``-a``. Old-style options can be more than one character long, are preceded by a single hyphen and may not be grouped together. Option arguments are specified by default following a space (``-foo null``) or after ``=`` (``-foo=null``).

- GNU-style long options, like ``--colors``. GNU-style long options can be more than one character long, are preceded by two hyphens, and can't be grouped together. Option arguments may be specified after a ``=`` (``--quoting-style=shell``), or, if ``--require-parameter`` is given, in the following parameter (``--quoting-style shell``).

Multiple commands and paths can be given in one call to define the same completions for multiple commands.

Multiple command switches and wrapped commands can also be given to define multiple completions in one call.

Invoking ``complete`` multiple times for the same command adds the new definitions on top of any existing completions defined for the command.

When ``-a`` or ``--arguments`` is specified in conjunction with long, short, or old-style options, the specified arguments are only completed as arguments for any of the specified options. If ``-a`` or ``--arguments`` is specified without any long, short, or old-style options, the specified arguments are used when completing non-option arguments to the command (except when completing an option argument that was specified with ``-r`` or ``--require-parameter``).

Command substitutions found in ``ARGUMENTS`` should return a newline-separated list of arguments, and each argument may optionally have a tab character followed by the argument description. Description given this way override a description given with ``-d`` or ``--description``.

Descriptions given with ``--description`` are also used to group options given with ``-s``, ``-o`` or ``-l``. Options with the same (non-empty) description will be listed as one candidate, and one of them will be picked. If the description is empty or no description was given this is skipped.

The ``-w`` or ``--wraps`` options causes the specified command to inherit completions from another command, "wrapping" the other command. The wrapping command can also have additional completions. A command can wrap multiple commands, and wrapping is transitive: if A wraps B, and B wraps C, then A automatically inherits all of C's completions. Wrapping can be removed using the ``-e`` or ``--erase`` options. Wrapping only works for completions specified with ``-c`` or ``--command`` and are ignored when specifying completions with ``-p`` or ``--path``.

When erasing completions, it is possible to either erase all completions for a specific command by specifying ``complete -c COMMAND -e``, or by specifying a specific completion option to delete.

When ``complete`` is called without anything that would define or erase completions (options, arguments, wrapping, ...), it shows matching completions instead. So ``complete`` without any arguments shows all loaded completions, ``complete -c foo`` shows all loaded completions for ``foo``. Since completions are :ref:`autoloaded <syntax-function-autoloading>`, you will have to trigger them first.

.. _completions-cygwin:

Cygwin / MSYS2 / Windows
------------------------

On Windows, binary executables have a ``.exe`` extension, but this extension is not required when calling an application (and if the name is not ambiguous, i.e. there isn't also a script called ``myprog`` in the same directory as ``myprog.exe``).

To unify completions between Windows and other OSes, on Cygwin/MSYS2/Windows, *COMMAND* does not require the ``.exe`` extension.
Completions for ``myprog`` will also be used for ``myprog.exe`` if there are no ambiguities, i.e. if there are no completions for ``myprog.exe`` specifically.
However, completions for ``myprog.exe`` will only be used when also using the ``.exe`` extension on the command line.

In other words:

::

    complete -c myprog.exe ...  #1

will only work for ``myprog.exe``

::

    complete -c myprog ...  #2

can work for both ``myprog`` and ``myprog.exe``. But if both completions exist, #2 will only be used for ``myprog`` while ``myprog.exe`` will use #1.

Examples
--------

The short-style option ``-o`` for the ``gcc`` command needs a file argument:

::

    complete -c gcc -s o -r


The short-style option ``-d`` for the ``grep`` command requires one of ``read``, ``skip`` or ``recurse``:

::

    complete -c grep -s d -x -a "read skip recurse"


The ``su`` command takes any username as an argument. Usernames are given as the first colon-separated field in the file /etc/passwd. This can be specified as:

::

    complete -x -c su -d "Username" -a "(cat /etc/passwd | cut -d : -f 1)"


The ``rpm`` command has several different modes. If the ``-e`` or ``--erase`` flag has been specified, ``rpm`` should delete one or more packages, in which case several switches related to deleting packages are valid, like the ``nodeps`` switch.

This can be written as:

::

    complete -c rpm -n "__fish_contains_opt -s e erase" -l nodeps -d "Don't check dependencies"


where ``__fish_contains_opt`` is a function that checks the command line buffer for the presence of a specified set of options.

To implement an alias, use the ``-w`` or ``--wraps`` option:



::

    complete -c hub -w git


Now hub inherits all of the completions from git. Note this can also be specified in a function declaration (``function thing -w otherthing``).

::

   complete -c git

Shows all completions for ``git``.

Any command ``foo`` that doesn't support grouping multiple short options in one string (not supporting ``-xf`` as short for ``-x -f``) or a short option and its value in one string (not supporting ``-d9`` instead of ``-d 9``) should be specified as a single-character old-style option instead of as a short-style option; for example, ``complete -c foo -o s; complete -c foo -o v`` would never suggest ``foo -ov`` but rather ``foo -o -v``.
