.. _cmd-complete:

complete - edit command specific tab-completions
================================================

Synopsis
--------

::

  complete ( -c | --command | -p | --path ) COMMAND
          [( -c | --command | -p | --path ) COMMAND]...
          [( -e | --erase )]
          [( -s | --short-option ) SHORT_OPTION]...
          [( -l | --long-option | -o | --old-option ) LONG_OPTION]...
          [( -a | --arguments ) OPTION_ARGUMENTS]
          [( -k | --keep-order )]
          [( -f | --no-files )]
          [( -F | --force-files )]
          [( -r | --require-parameter )]
          [( -x | --exclusive )]
          [( -w | --wraps ) WRAPPED_COMMAND]...
          [( -n | --condition ) CONDITION]
          [( -d | --description ) DESCRIPTION]
  complete ( -C [STRING] | --do-complete[=STRING] )

Description
-----------

For an introduction to specifying completions, see :ref:`Writing your own completions <completion-own>` in
the fish manual.

- ``COMMAND`` is the name of the command for which to add a completion.

- ``SHORT_OPTION`` is a one character option for the command.

- ``LONG_OPTION`` is a multi character option for the command.

- ``OPTION_ARGUMENTS`` is parameter containing a space-separated list of possible option-arguments, which may contain command substitutions.

- ``DESCRIPTION`` is a description of what the option and/or option arguments do.

- ``-c COMMAND`` or ``--command COMMAND`` specifies that ``COMMAND`` is the name of the command.

- ``-p COMMAND`` or ``--path COMMAND`` specifies that ``COMMAND`` is the absolute path of the program (optionally containing wildcards).

- ``-e`` or ``--erase`` deletes the specified completion.

- ``-s SHORT_OPTION`` or ``--short-option=SHORT_OPTION`` adds a short option to the completions list.

- ``-l LONG_OPTION`` or ``--long-option=LONG_OPTION`` adds a GNU style long option to the completions list.

- ``-o LONG_OPTION`` or ``--old-option=LONG_OPTION`` adds an old style long option to the completions list (See below for details).

- ``-a OPTION_ARGUMENTS`` or ``--arguments=OPTION_ARGUMENTS`` adds the specified option arguments to the completions list.

- ``-k`` or ``--keep-order`` preserves the order of the ``OPTION_ARGUMENTS`` specified via ``-a`` or ``--arguments`` instead of sorting alphabetically. Multiple ``complete`` calls with ``-k`` result in arguments of the later ones displayed first.

- ``-f`` or ``--no-files`` says that the options specified by this completion may not be followed by a filename.

- ``-F`` or ``--force-files`` says that the options specified by this completion may be followed by a filename, even if another applicable ``complete`` specified ``--no-files``.

- ``-r`` or ``--require-parameter`` says that the options specified by this completion must have an option argument, i.e. may not be followed by another option.

- ``-x`` or ``--exclusive`` implies both ``-r`` and ``-f``.

- ``-w WRAPPED_COMMAND`` or ``--wraps=WRAPPED_COMMAND`` causes the specified command to inherit completions from the wrapped command (See below for details).

- ``-n`` or ``--condition`` specifies a shell command that must return 0 if the completion is to be used. This makes it possible to specify completions that should only be used in some cases.

- ``-CSTRING`` or ``--do-complete=STRING`` makes complete try to find all possible completions for the specified string.

- ``-C`` or ``--do-complete`` with no argument makes complete try to find all possible completions for the current command line buffer. If the shell is not in interactive mode, an error is returned.

- ``-A`` and ``--authoritative`` no longer do anything and are silently ignored.

- ``-u`` and ``--unauthoritative`` no longer do anything and are silently ignored.

Command specific tab-completions in ``fish`` are based on the notion of options and arguments. An option is a parameter which begins with a hyphen, such as '``-h``', '``-help``' or '``--help``'. Arguments are parameters that do not begin with a hyphen. Fish recognizes three styles of options, the same styles as the GNU version of the getopt library. These styles are:

- Short options, like '``-a``'. Short options are a single character long, are preceded by a single hyphen and may be grouped together (like '``-la``', which is equivalent to '``-l -a``'). Option arguments may be specified in the following parameter ('``-w 32``') or by appending the option with the value ('``-w32``').

- Old style long options, like '``-Wall``'. Old style long options can be more than one character long, are preceded by a single hyphen and may not be grouped together. Option arguments are specified in the following parameter ('``-ao null``').

- GNU style long options, like '``--colors``'. GNU style long options can be more than one character long, are preceded by two hyphens, and may not be grouped together. Option arguments may be specified in the following parameter ('``--quoting-style shell``') or by appending the option with a '``=``' and the value ('``--quoting-style=shell``'). GNU style long options may be abbreviated so long as the abbreviation is unique ('``--h``') is equivalent to '``--help``' if help is the only long option beginning with an 'h').

The options for specifying command name and command path may be used multiple times to define the same completions for multiple commands.

The options for specifying command switches and wrapped commands may be used multiple times to define multiple completions for the command(s) in a single call.

Invoking ``complete`` multiple times for the same command adds the new definitions on top of any existing completions defined for the command.

When ``-a`` or ``--arguments`` is specified in conjunction with long, short, or old style options, the specified arguments are only used as completions when attempting to complete an argument for any of the specified options. If ``-a`` or ``--arguments`` is specified without any long, short, or old style options, the specified arguments are used when completing any argument to the command (except when completing an option argument that was specified with ``-r`` or ``--require-parameter``).

Command substitutions found in ``OPTION_ARGUMENTS`` are not expected to return a space-separated list of arguments. Instead they must return a newline-separated list of arguments, and each argument may optionally have a tab character followed by the argument description. Any description provided in this way overrides a description given with ``-d`` or ``--description``.

The ``-w`` or ``--wraps`` options causes the specified command to inherit completions from another command. The inheriting command is said to "wrap" the inherited command. The wrapping command may have its own completions in addition to inherited ones. A command may wrap multiple commands, and wrapping is transitive: if A wraps B, and B wraps C, then A automatically inherits all of C's completions. Wrapping can be removed using the ``-e`` or ``--erase`` options. Note that wrapping only works for completions specified with ``-c`` or ``--command`` and are ignored when specifying completions with ``-p`` or ``--path``.

When erasing completions, it is possible to either erase all completions for a specific command by specifying ``complete -c COMMAND -e``, or by specifying a specific completion option to delete by specifying either a long, short or old style option.


Example
-------

The short style option ``-o`` for the ``gcc`` command requires that a file follows it.  This can be done using writing:



::

    complete -c gcc -s o -r


The short style option ``-d`` for the ``grep`` command requires that one of the strings '``read``', '``skip``' or '``recurse``' is used.  This can be specified writing:



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


Now hub inherits all of the completions from git. Note this can also be specified in a function declaration.

