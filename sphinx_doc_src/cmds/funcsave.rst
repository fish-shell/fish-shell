.. _cmd-funcsave:

funcsave - save the definition of a function to the user's autoload directory
=============================================================================

Synopsis
--------

::

    funcsave FUNCTION_NAME


Description
-----------

``funcsave`` saves the current definition of a function to a file in the fish configuration directory. This function will be automatically loaded by current and future fish sessions. This can be useful if you have interactively created a new function and wish to save it for later use.

Note that because fish loads functions on-demand, saved functions will not function as :ref:`event handlers <event>` until they are run or sourced otherwise. To activate an event handler for every new shell, add the function to your :ref:`shell initialization file <initialization>` instead of using ``funcsave``.
