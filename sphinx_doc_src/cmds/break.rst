break - stop the current inner loop
===================================

Synopsis
--------

LOOP_CONSTRUCT; [COMMANDS...] break; [COMMANDS...] end


Description
-----------

``break`` halts a currently running loop, such as a `switch <cmds/switch.html>`__, `for <cmds/for.html>`__ or `while <cmds/while.html>`__ loop. It is usually added inside of a conditional block such as an `if <cmds/if.html`__ block.

There are no parameters for ``break``.


Example
-------
The following code searches all .c files for "smurf", and halts at the first occurrence.



::

    for i in *.c
        if grep smurf $i
            echo Smurfs are present in $i
            break
        end
    end

