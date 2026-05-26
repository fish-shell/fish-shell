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

Fish ships a wrapper function around the builtin ``value``. When the universal variable ``fish_value_show`` is set, stdout is a terminal, and ``value`` appears to be invoked interactively at the top level, the wrapper displays element count and per-element detail instead of raw output::

    > set -U fish_value_show 1
    > value $PATH
    3 elements
    [1]: |/usr/local/bin|
    [2]: |/usr/bin|
    [3]: |/bin|

Unset ``fish_value_show`` to disable this behavior. The plain builtin is always available via ``builtin value``.

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
