.. _cmd-not:

not - negate the exit status of a job
=====================================

Synopsis
--------

.. synopsis::

    not COMMAND [OPTIONS ...]


Description
-----------

``not`` negates the exit status of another command. If the exit status is zero, ``not`` returns 1. Otherwise, ``not`` returns 0.

The **-h** or **--help** option displays help about using this command.

Example
-------

The following code reports an error and exits if no file named spoon can be found.



::

    if not test -f spoon
        echo There is no spoon
        exit 1
    end


