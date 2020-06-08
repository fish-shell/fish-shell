.. _cmd-status:

status - query fish runtime information
=======================================

Synopsis
--------

::

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

The following operations (sub-commands) are available:

- ``is-command-substitution`` returns 0 if fish is currently executing a command substitution. Also ``-c`` or ``--is-command-substitution``.

- ``is-block`` returns 0 if fish is currently executing a block of code. Also ``-b`` or ``--is-block``.

- ``is-breakpoint`` returns 0 if fish is currently showing a prompt in the context of a ``breakpoint`` command. See also the ``fish_breakpoint_prompt`` function.

- ``is-interactive`` returns 0 if fish is interactive - that is, connected to a keyboard. Also ``-i`` or ``--is-interactive``.

- ``is-login`` returns 0 if fish is a login shell - that is, if fish should perform login tasks such as setting up the PATH. Also ``-l`` or ``--is-login``.

- ``is-full-job-control`` returns 0 if full job control is enabled. Also ``--is-full-job-control`` (no short flag).

- ``is-interactive-job-control`` returns 0 if interactive job control is enabled. Also, ``--is-interactive-job-control`` (no short flag).

- ``is-no-job-control`` returns 0 if no job control is enabled. Also ``--is-no-job-control`` (no short flag).

- ``current-command`` prints the name of the currently-running function or command, like the deprecated ``_`` variable.

- ``filename`` prints the filename of the currently running script. Also ``current-filename``, ``-f`` or ``--current-filename``. This depends on how the script was called - if it was called via a symlink, the symlink will be returned, and if the current script was received via ``source`` it will be ``-``.

- ``basename`` prints just the filename of the running script, without any path-components before.

- ``dirname`` prints just the path to the running script, without the actual filename itself. This can be relative to $PWD (including just "."), depending on how the script was called. This is the same as passing the ``filename`` to ``dirname(3)``. It's useful if you want to use other files in the current script's directory or similar.

- ``fish-path`` prints the absolute path to the currently executing instance of fish.

- ``function`` prints the name of the currently called function if able, when missing displays "Not a
  function" (or equivalent translated string). Also ``current-function``, ``-u`` or ``--current-function``.

- ``line-number`` prints the line number of the currently running script. Also ``current-line-number``, ``-n`` or ``--current-line-number``.

- ``stack-trace`` prints a stack trace of all function calls on the call stack. Also ``print-stack-trace``, ``-t`` or ``--print-stack-trace``.

- ``job-control CONTROL_TYPE`` sets the job control type, which can be ``none``, ``full``, or ``interactive``. Also ``-j CONTROL_TYPE`` or ``--job-control CONTROL_TYPE``.

- ``features`` lists all available feature flags.

- ``test-feature FEATURE`` returns 0 when FEATURE is enabled, 1 if it is disabled, and 2 if it is not recognized.

Notes
-----

For backwards compatibility each subcommand can also be specified as a long or short option. For example, rather than ``status is-login`` you can type ``status --is-login``. The flag forms are deprecated and may be removed in a future release (but not before fish 3.0).

You can only specify one subcommand per invocation even if you use the flag form of the subcommand.
