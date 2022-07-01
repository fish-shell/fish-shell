.. _cmd-status:

status - query fish runtime information
=======================================

Synopsis
--------

.. synopsis::

    status
    status is-login
    status is-interactive
    status is-block
    status is-breakpoint
    status is-command-substitution
    status is-no-job-control
    status is-full-job-control
    status is-interactive-job-control
    status current-command
    status filename
    status basename
    status dirname
    status fish-path
    status function
    status line-number
    status stack-trace
    status job-control CONTROL_TYPE
    status features
    status test-feature FEATURE

Description
-----------

With no arguments, ``status`` displays a summary of the current login and job control status of the shell.

The following operations (subcommands) are available:

**is-command-substitution**, **-c** or **--is-command-substitution**
    Returns 0 if fish is currently executing a command substitution.

**is-block**, **-b** or **--is-block**
    Returns 0 if fish is currently executing a block of code.

**is-breakpoint**
    Returns 0 if fish is currently showing a prompt in the context of a :ref:`breakpoint <cmd-breakpoint>` command. See also the :ref:`fish_breakpoint_prompt <cmd-fish_breakpoint_prompt>` function.

**is-interactive**, **-i** or **--is-interactive**
    Returns 0 if fish is interactive - that is, connected to a keyboard.

**is-login**, **-l** or **--is-login**
    Returns 0 if fish is a login shell - that is, if fish should perform login tasks such as setting up :envvar:`PATH`.

**is-full-job-control** or **--is-full-job-control**
    Returns 0 if full job control is enabled.

**is-interactive-job-control** or **--is-interactive-job-control**
    Returns 0 if interactive job control is enabled.

**is-no-job-control** or **--is-no-job-control**
    Returns 0 if no job control is enabled.

**current-command**
    Prints the name of the currently-running function or command, like the deprecated :envvar:`_` variable.

**filename**, **current-filename**, **-f** or **--current-filename**
    Prints the filename of the currently-running script. If the current script was called via a symlink, this will return the symlink. If the current script was received by piping into :ref:`source <cmd-source>`, then this will return ``-``.

**basename**
    Prints just the filename of the running script, without any path components before.

**dirname**
    Prints just the path to the running script, without the actual filename itself. This can be relative to :envvar:`PWD` (including just "."), depending on how the script was called. This is the same as passing the filename to ``dirname(3)``. It's useful if you want to use other files in the current script's directory or similar.

**fish-path**
    Prints the absolute path to the currently executing instance of fish. This is a best-effort attempt and the exact output is down to what the platform gives fish. In some cases you might only get "fish".

**function** or **current-function**
    Prints the name of the currently called function if able, when missing displays "Not a function" (or equivalent translated string).

**line-number**, **current-line-number**, **-n** or **--current-line-number**
    Prints the line number of the currently running script.

**stack-trace**, **print-stack-trace**, **-t** or **--print-stack-trace**
    Prints a stack trace of all function calls on the call stack.

**job-control**, **-j** or **--job-control** *CONTROL_TYPE*
    Sets the job control type to *CONTROL_TYPE*, which can be **none**, **full**, or **interactive**.

**features**
    Lists all available feature flags.

**test-feature** *FEATURE*
    Returns 0 when FEATURE is enabled, 1 if it is disabled, and 2 if it is not recognized.

Notes
-----

For backwards compatibility most subcommands can also be specified as a long or short option. For example, rather than ``status is-login`` you can type ``status --is-login``. The flag forms are deprecated and may be removed in a future release (but not before fish 4.0).

You can only specify one subcommand per invocation even if you use the flag form of the subcommand.
