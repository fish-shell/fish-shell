.. _cmd-not:

not - negate the exit status of a job
=====================================

Synopsis
--------

::

    not COMMAND [OPTIONS...]


Description
-----------

``not`` negates the exit status of another command. If the exit status is zero, ``not`` returns 1. Otherwise, ``not`` returns 0.


Example
-------

The following code reports an error and exits if no file named spoon can be found.



::

    if not test -f spoon
        echo There is no spoon
        exit 1
    end


