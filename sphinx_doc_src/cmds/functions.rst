.. _cmd-functions:

functions - print or erase functions
====================================

Synopsis
--------

::

    functions [ -a | --all ] [ -n | --names ]
    functions [ -D | --details ] [ -v ] FUNCTION
    functions -c OLDNAME NEWNAME
    functions -d DESCRIPTION FUNCTION
    functions [ -e | -q ] FUNCTIONS...

Description
-----------

``functions`` prints or erases functions.

The following options are available:

- ``-a`` or ``--all`` lists all functions, even those whose name starts with an underscore.

- ``-c OLDNAME NEWNAME`` or ``--copy OLDNAME NEWNAME`` creates a new function named NEWNAME, using the definition of the OLDNAME function.

- ``-d DESCRIPTION`` or ``--description=DESCRIPTION`` changes the description of this function.

- ``-e`` or ``--erase`` causes the specified functions to be erased. This also means that it is prevented from autoloading.

- ``-D`` or ``--details`` reports the path name where each function is defined or could be autoloaded, ``stdin`` if the function was defined interactively or on the command line or by reading stdin, ``-`` if the function was created via ``source``, and ``n/a`` if the function isn't available. (Functions created via ``alias`` will return ``-``, because ``alias`` uses ``source`` internally.) If the ``--verbose`` option is also specified then five lines are written:

    - the pathname as already described,
    - ``autoloaded``, ``not-autoloaded`` or ``n/a``,
    - the line number within the file or zero if not applicable,
    - ``scope-shadowing`` if the function shadows the vars in the calling function (the normal case if it wasn't defined with ``--no-scope-shadowing``), else ``no-scope-shadowing``, or ``n/a`` if the function isn't defined,
    - the function description minimally escaped so it is a single line or ``n/a`` if the function isn't defined.

You should not assume that only five lines will be written since we may add additional information to the output in the future.

- ``-n`` or ``--names`` lists the names of all defined functions.

- ``-q`` or ``--query`` tests if the specified functions exist.

- ``-v`` or ``--verbose`` will make some output more verbose.

- ``-H`` or ``--handlers`` will show all event handlers.

- ``-t`` or ``--handlers-type TYPE`` will show all event handlers matching the given type

The default behavior of ``functions``, when called with no arguments, is to print the names of all defined functions. Unless the ``-a`` option is given, no functions starting with underscores are not included in the output.

If any non-option parameters are given, the definition of the specified functions are printed.

Automatically loaded functions cannot be removed using ``functions -e``. Either remove the definition file or change the $fish_function_path variable to remove autoloaded functions.

Copying a function using ``-c`` copies only the body of the function, and does not attach any event notifications from the original function.

Only one function's description can be changed in a single invocation of ``functions -d``.

The exit status of ``functions`` is the number of functions specified in the argument list that do not exist, which can be used in concert with the ``-q`` option.


Examples
--------


::

    functions -n
    # Displays a list of currently-defined functions
    
    functions -c foo bar
    # Copies the 'foo' function to a new function called 'bar'
    
    functions -e bar
    # Erases the function ``bar``

