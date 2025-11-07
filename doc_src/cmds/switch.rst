switch - conditionally execute a block of commands
==================================================

Synopsis
--------

.. synopsis::

    switch VALUE; [case [GLOB ...]; [COMMANDS ...]; ...] end

Description
-----------

``switch`` performs one of several blocks of commands, depending on whether a specified value equals one of several globbed values. ``case`` is used together with the ``switch`` statement in order to determine which block should be executed.

Each ``case`` command is given one or more parameters. The first ``case`` command with a parameter that matches the string specified in the switch command will be evaluated. ``case`` parameters may contain globs. These need to be escaped or quoted in order to avoid regular glob expansion using filenames.

Note that fish does not fall through on case statements. Only the first matching case is executed.

Note that :doc:`break <break>` cannot be used to exit a case/switch block early like in other languages. It can only be used in loops.

Note that command substitutions in a case statement will be evaluated even if its body is not taken. All substitutions, including command substitutions, must be performed before the value can be compared against the parameter.


Example
-------

If the variable ``$animal`` contains the name of an animal, the following code would attempt to classify it:

::

    switch $animal
        case cat
            echo evil
        case wolf dog human moose dolphin whale
            echo mammal
        case duck goose albatross
            echo bird
        case shark trout stingray
            echo fish
        case '*'
            echo I have no idea what a $animal is
    end


If the above code was run with ``$animal`` set to ``whale``, the output
would be ``mammal``.
