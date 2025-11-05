cdh - change to a recently visited directory
============================================

Synopsis
--------

.. synopsis::

    cdh [DIRECTORY]

Description
-----------

``cdh`` with no arguments presents a list of :ref:`recently visited directories <directory-history>`.
You can then select one of the entries by letter or number.
You can also press :kbd:`tab` to use the completion pager to select an item from the list.
If you give it a single argument it is equivalent to ``cd DIRECTORY``.

Note that the ``cd`` command limits directory history to the 25 most recently visited directories.
The history is stored in the :envvar:`dirprev` and :envvar:`dirnext` variables, which this command manipulates.
If you make those universal variables, your ``cd`` history is shared among all fish instances.

See Also
--------

- the :doc:`dirh <dirh>` command to print the directory history
- the :doc:`prevd <prevd>` command to move backward
- the :doc:`nextd <nextd>` command to move forward
