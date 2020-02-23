.. _cmd-or:

or - conditionally execute a command
====================================

Synopsis
--------

::

    COMMAND1; or COMMAND2

Description
-----------

``or`` is used to execute a command if the previous command was not successful (returned a status of something other than 0).

``or`` statements may be used as part of the condition in an :ref:`and <cmd-if>` or :ref:`while <cmd-while>` block.

``or`` does not change the current exit status itself, but the command it runs most likely will. The exit status of the last foreground command to exit can always be accessed using the :ref:`$status <variables-status>` variable.

Example
-------

The following code runs the ``make`` command to build a program. If the build succeeds, the program is installed. If either step fails, ``make clean`` is run, which removes the files created by the build process.

::

    make; and make install; or make clean

See Also
--------

- :ref:`and <cmd-and>` command
