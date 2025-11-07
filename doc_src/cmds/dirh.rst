dirh - print directory history
==============================

Synopsis
--------

.. synopsis::

    dirh

Description
-----------

``dirh`` prints the current :ref:`directory history <directory-history>`. The current position in the history is highlighted using the color defined in the ``fish_color_history_current`` environment variable.

``dirh`` does not accept any parameters.

Note that the :doc:`cd <cd>` command limits directory history to the 25 most recently visited directories. The history is stored in the ``$dirprev`` and ``$dirnext`` variables.

See Also
--------

- the :doc:`cdh <cdh>` command to display a prompt to quickly navigate the history
- the :doc:`prevd <prevd>` command to move backward
- the :doc:`nextd <nextd>` command to move forward
