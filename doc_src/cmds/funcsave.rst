funcsave - save the definition of a function to the user's autoload directory
=============================================================================

Synopsis
--------

.. synopsis::

    funcsave FUNCTION_NAME
    funcsave [-q | --quiet] [(-d | --directory) DIR] FUNCTION_NAME


Description
-----------

``funcsave`` saves a function to a file in the fish configuration directory. This function will be :ref:`automatically loaded <syntax-function-autoloading>` by current and future fish sessions. This can be useful to commit functions created interactively for permanent use.

If you have erased a function using :doc:`functions <functions>`'s ``--erase`` option, ``funcsave`` will remove the saved function definition.

Because fish loads functions on-demand, saved functions cannot serve as :ref:`event handlers <event>` until they are run or otherwise sourced. To activate an event handler for every new shell, add the function to the :ref:`configuration file <configuration>` instead of using ``funcsave``.

This is often used after :doc:`funced <funced>`, which opens the function in ``$EDITOR`` or ``$VISUAL`` and loads it into the current session afterwards.

To view a function's current definition, use :doc:`functions <functions>` or :doc:`type <type>`.
