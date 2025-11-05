while - perform a set of commands multiple times
================================================

Synopsis
--------

.. synopsis::

    while CONDITION; COMMANDS; end

Description
-----------

**while** repeatedly executes ``CONDITION``, and if the exit status is 0, then executes ``COMMANDS``.

The exit status of the **while** loop is the exit status of the last iteration of the ``COMMANDS`` executed, or 0 if none were executed. (This matches other shells and is POSIX-compatible.)

You can use :doc:`and <and>` or :doc:`or <or>` for complex conditions. Even more complex control can be achieved with ``while true`` containing a :doc:`break <break>`.

The **-h** or **--help** option displays help about using this command.

Example
-------

::

    while test -f foo.txt; or test -f bar.txt ; echo file exists; sleep 10; end
    # outputs 'file exists' at 10 second intervals,
    # as long as the file foo.txt or bar.txt exists.

