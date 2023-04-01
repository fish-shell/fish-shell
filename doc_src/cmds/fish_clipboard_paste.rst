.. _cmd-ghoti_clipboard_paste:

ghoti_clipboard_paste - get text from the system's clipboard
==============================================================

Synopsis
--------

.. synopsis::

    ghoti_clipboard_paste

    ghoti_clipboard_paste | foo

Description
-----------

The ``ghoti_clipboard_paste`` function copies text from the system clipboard.

If its stdout is not a terminal (see :doc:`isatty <isatty>`), it will output everything there, as-is, without any additional newlines. If it is, it will put the text in the commandline instead.

If it outputs to the commandline, it will automatically escape the output if the cursor is currently inside single-quotes so it is suitable for single-quotes (meaning it escapes ``'`` and ``\\``).

It is bound to :kbd:`Control`\ +\ :kbd:`V` by default.

``ghoti_clipboard_paste`` works by calling a system-specific backend. If it doesn't appear to work you may need to install yours.

Currently supported are:

- ``pbpaste``
- ``wl-paste`` using wayland
- ``xsel`` and ``xclip`` for X11
- ``powershell.exe`` on Windows (this backend has encoding limitations and uses windows line endings that ``ghoti_clipboard_paste`` undoes)

See also
--------

- :doc:`ghoti_clipboard_copy` which does the inverse.
