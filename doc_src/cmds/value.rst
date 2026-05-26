value - output arguments as separate list elements
===================================================

Synopsis
--------

.. synopsis::

    value [VALUE ...]

Description
-----------

``value`` outputs each of its arguments as a separate list element, preserving each as a distinct element when captured in a command substitution::

    set mode (value read write exec)[$n]
    set separator (value $_flag_separator ' ')[1]
    set merged (value $xs $ys | sort -u)

``value`` accepts no options, reads no stdin, and always exits with status 0.

Like :doc:`set <set>`, :doc:`path <path>`, and :doc:`count <count>`, ``value`` uses "nullglob" behavior: if a wildcard argument fails to expand, it silently expands to nothing rather than aborting the command.

Examples
--------

Start a pipeline from a variable::

    value $items | sort -u

    value {foo,bar,baz}.txt | xargs -l1 cat

Construct a temporary list for an environment override::

    PATH=(value ~/.bin /usr/local/bin) somecommand

Apply a default to an argparse flag::

    set separator (value $_flag_separator ' ')[1]

Return collected results from a function::

    function myxargs
        while read input
            set -a outputs ($argv (string split ' ' -- $input))
        end
        value $outputs
    end

See Also
--------

- the :doc:`echo <echo>` command, for more control over output formatting
- the :doc:`string collect <string-collect>` subcommand
- the :doc:`count <count>` command
