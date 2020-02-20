.. _cmd-dirh:

dirh - print directory history
==============================

Synopsis
--------

::

    dirh

Description
-----------

``dirh`` prints the current directory history. The current position in the history is highlighted using the color defined in the ``fish_color_history_current`` environment variable.

``dirh`` does not accept any parameters.

Note that the ``cd`` command limits directory history to the 25 most recently visited directories. The history is stored in the ``$dirprev`` and ``$dirnext`` variables.
