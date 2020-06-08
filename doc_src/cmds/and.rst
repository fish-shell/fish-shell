.. _cmd-and:

and - conditionally execute a command
=====================================

Synopsis
--------

::

    COMMAND1; and COMMAND2

Description
-----------

``and`` is used to execute a command if the previous command was successful (returned a status of 0).

``and`` statements may be used as part of the condition in an :ref:`while <cmd-while>` or :ref:`if <cmd-if>` block.

``and`` does not change the current exit status itself, but the command it runs most likely will. The exit status of the last foreground command to exit can always be accessed using the :ref:`$status <variables-status>` variable.

Example
-------

The following code runs the ``make`` command to build a program. If the build succeeds, ``make``'s exit status is 0, and the program is installed. If either step fails, the exit status is 1, and ``make clean`` is run, which removes the files created by the build process.

::

    make; and make install; or make clean

See Also
--------

- :ref:`or <cmd-or>` command
