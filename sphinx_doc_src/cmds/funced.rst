.. _cmd-funced:

funced - edit a function interactively
======================================

Synopsis
--------

::

    funced [OPTIONS] NAME

Description
-----------

``funced`` provides an interface to edit the definition of the function ``NAME``.

If the ``$VISUAL`` environment variable is set, it will be used as the program to edit the function. If ``$VISUAL`` is unset but ``$EDITOR`` is set, that will be used. Otherwise, a built-in editor will be used. Note that to enter a literal newline using the built-in editor you should press :kbd:`Alt+Enter`. Pressing :kbd:`Enter` signals that you are done editing the function. This does not apply to an external editor like emacs or vim.

If there is no function called ``NAME`` a new function will be created with the specified name

- ``-e command`` or ``--editor command`` Open the function body inside the text editor given by the command (for example, ``-e vi``). The special command ``fish`` will use the built-in editor (same as specifying ``-i``).

- ``-i`` or ``--interactive`` Force opening the function body in the built-in editor even if ``$VISUAL`` or ``$EDITOR`` is defined.

- ``-s`` or ``--save`` Automatically save the function after successfully editing it.
