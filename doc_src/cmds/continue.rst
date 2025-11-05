continue - skip the remainder of the current iteration of the current inner loop
================================================================================

Synopsis
--------

.. synopsis::

    LOOP_CONSTRUCT; [COMMANDS ...;] continue; [COMMANDS ...;] end

Description
-----------

``continue`` skips the remainder of the current iteration of the current inner loop, such as a :doc:`for <for>` loop or a :doc:`while <while>` loop. It is usually added inside of a conditional block such as an :doc:`if <if>` statement or a :doc:`switch <switch>` statement.

The **-h** or **--help** option displays help about using this command.

Example
-------

The following code removes all tmp files that do not contain the word smurf.

::

    for i in *.tmp
        if grep smurf $i
            continue
        end
        # This "rm" is skipped over if "continue" is executed.
        rm $i
        # As is this "echo"
        echo $i
    end

See Also
--------

- the :doc:`break <break>` command, to stop the current inner loop
