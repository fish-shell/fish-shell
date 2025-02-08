.. _cmd-fish_clipboard_copy:

fish_clipboard_copy - copy text to the system's clipboard
==============================================================

Synopsis
--------

.. synopsis::

    fish_clipboard_copy

    foo | fish_clipboard_copy

Description
-----------

The ``fish_clipboard_copy`` function copies text to the system clipboard.

If stdin is not a terminal (see :doc:`isatty <isatty>`), it will read all input from there and copy it. If it is, it will use the current commandline, or the current selection if there is one.

It is bound to :kbd:`ctrl-x` by default.

``fish_clipboard_copy`` works by calling a system-specific backend. If it doesn't appear to work you may need to install yours.

Currently supported are:

- ``pbcopy``
- ``wl-copy`` using wayland
- ``xsel`` and ``xclip`` for X11
- ``clip.exe`` on Windows.
- The OSC 52 clipboard sequence, which your terminal might support

See also
--------

- :doc:`fish_clipboard_paste` which does the inverse.
