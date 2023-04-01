.. _cmd-ghoti_clipboard_copy:

ghoti_clipboard_copy - copy text to the system's clipboard
==============================================================

Synopsis
--------

.. synopsis::

    ghoti_clipboard_copy

    foo | ghoti_clipboard_copy

Description
-----------

The ``ghoti_clipboard_copy`` function copies text to the system clipboard.

If stdin is not a terminal (see :doc:`isatty <isatty>`), it will read all input from there and copy it. If it is, it will use the current commandline, or the current selection if there is one.

It is bound to :kbd:`Control`\ +\ :kbd:`X` by default.

``ghoti_clipboard_copy`` works by calling a system-specific backend. If it doesn't appear to work you may need to install yours.

Currently supported are:

- ``pbcopy``
- ``wl-copy`` using wayland
- ``xsel`` and ``xclip`` for X11
- ``clip.exe`` on Windows.

See also
--------

- :doc:`ghoti_clipboard_paste` which does the inverse.
