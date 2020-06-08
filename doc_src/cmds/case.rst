.. _cmd-case:

case - conditionally execute a block of commands
================================================

Synopsis
--------

::

    switch VALUE; [case [WILDCARD...]; [COMMANDS...]; ...] end

Description
-----------

``switch`` executes one of several blocks of commands, depending on whether a specified value matches one of several values. ``case`` is used together with the ``switch`` statement in order to determine which block should be executed.

Each ``case`` command is given one or more parameters. The first ``case`` command with a parameter that matches the string specified in the switch command will be evaluated. ``case`` parameters may contain wildcards. These need to be escaped or quoted in order to avoid regular wildcard expansion using filenames.

Note that fish does not fall through on case statements. Only the first matching case is executed.

Note that command substitutions in a case statement will be evaluated even if its body is not taken. All substitutions, including command substitutions, must be performed before the value can be compared against the parameter.

Example
-------

Say \$animal contains the name of an animal. Then this code would classify it:

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
        # Note that the next case has a wildcard which is quoted
        case '*'
            echo I have no idea what a $animal is
    end


If the above code was run with ``$animal`` set to ``whale``, the output
would be ``mammal``.

If ``$animal`` was set to "banana", it would print "I have no idea what a banana is".
