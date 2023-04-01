.. _cmd-ghoti:
.. program::ghoti

ghoti - the friendly interactive shell
=====================================

Synopsis
--------

.. synopsis::

    ghoti [OPTIONS] [FILE [ARG ...]]
    ghoti [OPTIONS] [-c COMMAND [ARG ...]]

Description
-----------

:command:`ghoti` is a command-line shell written mainly with interactive use in mind.
This page briefly describes the options for invoking :command:`ghoti`.
The :ref:`full manual <intro>` is available in HTML by using the :command:`help` command from inside ghoti, and in the `ghoti-doc(1)` man page.
The :ref:`tutorial <tutorial>` is available as HTML via ``help tutorial`` or in `man ghoti-tutorial`.


The following options are available:

**-c** or **--command=COMMAND**
    Evaluate the specified commands instead of reading from the commandline, passing additional positional arguments through ``$argv``.

**-C** or **--init-command=COMMANDS**
    Evaluate specified commands after reading the configuration but before executing command specified by **-c** or reading interactive input.

**-d** or **--debug=DEBUG_CATEGORIES**
    Enables debug output and specify a pattern for matching debug categories.
    See :ref:`Debugging <debugging-ghoti>` below for details.

**-o** or **--debug-output=DEBUG_FILE**
    Specifies a file path to receive the debug output, including categories and  :envvar:`ghoti_trace`.
    The default is stderr.

**-i** or **--interactive**
    The shell is interactive.

**-l** or **--login**
    Act as if invoked as a login shell.

**-N** or **--no-config**
    Do not read configuration files.

**-n** or **--no-execute**
    Do not execute any commands, only perform syntax checking.

**-p** or **--profile=PROFILE_FILE**
    when :command:`ghoti` exits, output timing information on all executed commands to the specified file.
    This excludes time spent starting up and reading the configuration.

**--profile-startup=PROFILE_FILE** 
    Will write timing for ``ghoti`` startup to specified file.

**-P** or **--private**
    Enables :ref:`private mode <private-mode>`: **ghoti** will not access old or store new history.

**--print-rusage-self**
    When :command:`ghoti` exits, output stats from getrusage.

**--print-debug-categories**
    Print all debug categories, and then exit.

**-v** or **--version**
    Print version and exit.

**-f** or **--features=FEATURES**
    Enables one or more comma-separated :ref:`feature flags <featureflags>`.

The ``ghoti`` exit status is generally the :ref:`exit status of the last foreground command <variables-status>`.

.. _debugging-ghoti:

Debugging
---------

While ghoti provides extensive support for :ref:`debugging ghoti scripts <debugging>`, it is also possible to debug and instrument its internals.
Debugging can be enabled by passing the **--debug** option.
For example, the following command turns on debugging for background IO thread events, in addition to the default categories, i.e. *debug*, *error*, *warning*, and *warning-path*:
::

    > ghoti --debug=iothread

Available categories are listed by ``ghoti --print-debug-categories``. The **--debug** option accepts a comma-separated list of categories, and supports glob syntax.
The following command turns on debugging for *complete*, *history*, *history-file*, and *profile-history*, as well as the default categories:
::

    > ghoti --debug='complete,*history*'

Debug messages output to stderr by default. Note that if :envvar:`ghoti_trace` is set, execution tracing also outputs to stderr by default. You can output to a file using the **--debug-output** option:
::

    > ghoti --debug='complete,*history*' --debug-output=/tmp/ghoti.log --init-command='set ghoti_trace on'

These options can also be changed via the :envvar:`FISH_DEBUG` and :envvar:`FISH_DEBUG_OUTPUT` variables.
The categories enabled via **--debug** are *added* to the ones enabled by $FISH_DEBUG, so they can be disabled by prefixing them with **-** (**reader-*,-ast*** enables reader debugging and disables ast debugging).

The file given in **--debug-output** takes precedence over the file in :envvar:`FISH_DEBUG_OUTPUT`.
