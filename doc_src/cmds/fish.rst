.. _cmd-fish:

fish - the friendly interactive shell
=====================================

Synopsis
--------

::

    fish [OPTIONS] [-c command] [FILE [ARGUMENTS...]]

Description
-----------

``fish`` is a command-line shell written mainly with interactive use in mind. This page briefly describes the options for invoking fish. The :ref:`full manual <intro>` is available in HTML by using the :ref:`help <cmd-help>` command from inside fish, and in the `fish-doc(1)` man page. The :ref:`tutorial <tutorial>` is available as HTML via ``help tutorial`` or in `fish-tutorial(1)`.

The following options are available:

- ``-c`` or ``--command=COMMANDS`` evaluate the specified commands instead of reading from the commandline

- ``-C`` or ``--init-command=COMMANDS`` evaluate the specified commands after reading the configuration, before running the command specified by ``-c`` or reading interactive input

- ``-d`` or ``--debug=CATEGORY_GLOB`` enables debug output and specifies a glob for matching debug categories (like ``fish -d``). Defaults to empty.

- ``-o`` or ``--debug-output=path`` Specify a file path to receive the debug output, including categories and ``fish_trace``. The default is stderr.

- ``-i`` or ``--interactive`` specify that fish is to run in interactive mode

- ``-l`` or ``--login`` specify that fish is to run as a login shell

- ``-n`` or ``--no-execute`` do not execute any commands, only perform syntax checking

- ``-p`` or ``--profile=PROFILE_FILE`` when fish exits, output timing information on all executed commands to the specified file

- ``-P`` or ``--private`` enables :ref:`private mode <private-mode>`, so fish will not access old or store new history.

- ``--print-rusage-self`` when fish exits, output stats from getrusage

- ``--print-debug-categories`` outputs the list of debug categories, and then exits.

- ``-v`` or ``--version`` display version and exit

- ``-D`` or ``--debug-stack-frames=DEBUG_LEVEL`` specify how many stack frames to display when debug messages are written. The default is zero. A value of 3 or 4 is usually sufficient to gain insight into how a given debug call was reached but you can specify a value up to 128.

- ``-f`` or ``--features=FEATURES`` enables one or more :ref:`feature flags <featureflags>` (separated by a comma). These are how fish stages changes that might break scripts.

The fish exit status is generally the exit status of the last foreground command. If fish is exiting because of a parse error, the exit status is 127.
