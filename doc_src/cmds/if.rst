.. _cmd-if:

if - conditionally execute a command
====================================

Synopsis
--------

::

    if CONDITION; COMMANDS_TRUE...;
    [else if CONDITION2; COMMANDS_TRUE2...;]
    [else; COMMANDS_FALSE...;]
    end

Description
-----------

``if`` will execute the command ``CONDITION``. If the condition's exit status is 0, the commands ``COMMANDS_TRUE`` will execute.  If the exit status is not 0 and :ref:`else <cmd-else>` is given, ``COMMANDS_FALSE`` will be executed.

You can use :ref:`and <cmd-and>` or :ref:`or <cmd-or>` in the condition. See the second example below.

The exit status of the last foreground command to exit can always be accessed using the :ref:`$status <variables-status>` variable.

Example
-------

The following code will print ``foo.txt exists`` if the file foo.txt exists and is a regular file, otherwise it will print ``bar.txt exists`` if the file bar.txt exists and is a regular file, otherwise it will print ``foo.txt and bar.txt do not exist``.



::

    if test -f foo.txt
        echo foo.txt exists
    else if test -f bar.txt
        echo bar.txt exists
    else
        echo foo.txt and bar.txt do not exist
    end


The following code will print "foo.txt exists and is readable" if foo.txt is a regular file and readable


::

    if test -f foo.txt
       and test -r foo.txt
       echo "foo.txt exists and is readable"
    end

