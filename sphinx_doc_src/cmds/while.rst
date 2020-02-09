.. _cmd-while:

while - perform a command multiple times
========================================

Synopsis
--------

::

    while CONDITION; COMMANDS...; end


Description
-----------

``while`` repeatedly executes ``CONDITION``, and if the exit status is 0, then executes ``COMMANDS``.

The exit status of the while loop is the exit status of the last iteration of the ``COMMANDS`` executed, or 0 if none were executed. (This matches other shells and is POSIX-compatible.)

You can use :ref:`and <cmd-and>` or :ref:`or <cmd-or>` for complex conditions. Even more complex control can be achieved with ``while true`` containing a :ref:`break <cmd-break>`.

Example
-------



::

    while test -f foo.txt; or test -f bar.txt ; echo file exists; sleep 10; end
    # outputs 'file exists' at 10 second intervals as long as the file foo.txt or bar.txt exists.

