.. _cmd-fish_key_reader:

fish_key_reader - explore what characters keyboard keys send
============================================================

Synopsis
--------

::

    fish_key_reader [OPTIONS]

Description
-----------

``fish_key_reader`` is used to study input received from the terminal and can help with key binds. The program is interactive and works on standard input. Individual characters themselves and their hexadecimal values are displayed.

The tool will write an example :ref:`bind <cmd-bind>` command matching the character sequence captured to stdout. If the character sequence matches a special key name (see ``bind --key-names``),  both ``bind CHARS ...`` and ``bind -k KEYNAME ...`` usage will be shown. Additional details about the characters received, such as the delay between chars, are written to stderr.

The following options are available:

- ``-c`` or ``--continuous`` begins a session where multiple key sequences can be inspected. By default the program exits after capturing a single key sequence.

- ``-h`` or ``--help`` prints usage information.

- ``-v`` or ``--version`` prints fish_key_reader's version and exits.

Usage Notes
-----------

The delay in milliseconds since the previous character was received is included in the diagnostic information written to stderr. This information may be useful to determine the optimal ``fish_escape_delay_ms`` setting or learn the amount of lag introduced by tools like ``ssh``, ``mosh`` or ``tmux``.

``fish_key_reader`` intentionally disables handling of many signals. To terminate ``fish_key_reader`` in ``--continuous`` mode do:

- press :kbd:`Control`\ +\ :kbd:`C` twice, or
- press :kbd:`Control`\ +\ :kbd:`D` twice, or
- type ``exit``, or
- type ``quit``
