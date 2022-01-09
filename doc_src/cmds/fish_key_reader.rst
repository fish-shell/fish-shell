.. _cmd-fish_key_reader:

fish_key_reader - explore what characters keyboard keys send
============================================================

Synopsis
--------

.. synopsis::

    fish_key_reader [OPTIONS]

Description
-----------

``fish_key_reader`` is used to explain how you would bind a certain key sequence. By default, it prints the :ref:`bind <cmd-bind>` command for one key sequence read interactively over standard input.

If the character sequence matches a special key name (see ``bind --key-names``),  both ``bind CHARS ...`` and ``bind -k KEYNAME ...`` usage will be shown. In verbose mode (enabled by passing ``--verbose``), additional details about the characters received, such as the delay between chars, are written to stderr.

The following options are available:

- ``-c`` or ``--continuous`` begins a session where multiple key sequences can be inspected. By default the program exits after capturing a single key sequence.

- ``-V`` or ``--verbose`` tells fish_key_reader to output timing information and explain the sequence in more detail.

- ``-h`` or ``--help`` prints usage information.

- ``-v`` or ``--version`` prints fish_key_reader's version and exits.

Usage Notes
-----------

In verbose mode, the delay in milliseconds since the previous character was received is included in the diagnostic information written to stderr. This information may be useful to determine the optimal ``fish_escape_delay_ms`` setting or learn the amount of lag introduced by tools like ``ssh``, ``mosh`` or ``tmux``.

``fish_key_reader`` intentionally disables handling of many signals. To terminate ``fish_key_reader`` in ``--continuous`` mode do:

- press :kbd:`Control`\ +\ :kbd:`C` twice, or
- press :kbd:`Control`\ +\ :kbd:`D` twice, or
- type ``exit``, or
- type ``quit``

Example
-------

::

   > fish_key_reader
   Press a key:
   # press up-arrow
   bind \e\[A 'do something'

   > fish_key_reader --verbose
   Press a key:
   # press alt+enter
              hex:   1B  char: \c[  (or \e)
   (  0.027 ms)  hex:    D  char: \cM  (or \r)
   bind \e\r 'do something'

