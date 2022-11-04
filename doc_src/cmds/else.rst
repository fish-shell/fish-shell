.. SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
.. SPDX-FileCopyrightText: © 2009 fish-shell contributors
.. SPDX-FileCopyrightText: © 2022 fish-shell contributors
..
.. SPDX-License-Identifier: GPL-2.0-only

.. _cmd-else:

else - execute command if a condition is not met
================================================

Synopsis
--------

.. synopsis::

    if CONDITION; COMMANDS_TRUE ...; [else; COMMANDS_FALSE ...;] end

Description
-----------

:doc:`if <if>` will execute the command *CONDITION**.
If the condition's exit status is 0, the commands *COMMANDS_TRUE* will execute.
If it is not 0 and **else** is given, *COMMANDS_FALSE* will be executed.


Example
-------

The following code tests whether a file *foo.txt* exists as a regular file.

::

    if test -f foo.txt
        echo foo.txt exists
    else
        echo foo.txt does not exist
    end

