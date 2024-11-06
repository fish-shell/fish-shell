.. _cmd-export:

export - compatibility function for exporting variables
=======================================================

Synopsis
--------

.. synopsis::

    export
    export NAME=VALUE


Description
-----------

``export`` is a function included for compatibility with POSIX shells. In general, the :doc:`set <set>` 
builtin should be used instead.

When called without arguments, ``export`` prints a list of currently-exported variables, like ``set
-x``.

When called with a ``NAME=VALUE`` pair, the variable ``NAME`` is set to ``VALUE`` in the global
scope, and exported as an environment variable to other commands.

There are no options available.

Example
-------

The following commands have an identical effect.

::

    set -gx PAGER bat
    export PAGER=bat

Note: If you want to add to e.g. ``$PATH``, you need to be careful to :ref:`combine the list <cartesian-product>`. Quote it, like so::

    export PATH="$PATH:/opt/bin"

Or just use ``set``, which avoids this::

    set -gx PATH $PATH /opt/bin

See more
--------

1. The :doc:`set <set>` command.
