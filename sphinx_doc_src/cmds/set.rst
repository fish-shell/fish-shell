.. _cmd-set:

set - display and change shell variables.
=========================================

Synopsis
--------

::

    set [SCOPE_OPTIONS]
    set [OPTIONS] VARIABLE_NAME VALUES...
    set [OPTIONS] VARIABLE_NAME[INDICES]... VALUES...
    set ( -q | --query ) [SCOPE_OPTIONS] VARIABLE_NAMES...
    set ( -e | --erase ) [SCOPE_OPTIONS] VARIABLE_NAME
    set ( -e | --erase ) [SCOPE_OPTIONS] VARIABLE_NAME[INDICES]...
    set ( -S | --show ) [VARIABLE_NAME]...

Description
-----------

``set`` manipulates :ref:`shell variables <variables>`.

If set is called with no arguments, the names and values of all shell variables are printed in sorted order. If some of the scope or export flags have been given, only the variables matching the specified scope are printed.

With both variable names and values provided, ``set`` assigns the variable ``VARIABLE_NAME`` the values ``VALUES...``.

The following options control variable scope:

- ``-a`` or ``--append`` causes the values to be appended to the current set of values for the variable. This can be used with ``--prepend`` to both append and prepend at the same time. This cannot be used when assigning to a variable slice.

- ``-p`` or ``--prepend`` causes the values to be prepended to the current set of values for the variable. This can be used with ``--append`` to both append and prepend at the same time. This cannot be used when assigning to a variable slice.

- ``-l`` or ``--local`` forces the specified shell variable to be given a scope that is local to the current block, even if a variable with the given name exists and is non-local

- ``-g`` or ``--global`` causes the specified shell variable to be given a global scope. Non-global variables disappear when the block they belong to ends

- ``-U`` or ``--universal`` causes the specified shell variable to be given a universal scope. If this option is supplied, the variable will be shared between all the current user's fish instances on the current computer, and will be preserved across restarts of the shell.

- ``-x`` or ``--export`` causes the specified shell variable to be exported to child processes (making it an "environment variable")

- ``-u`` or ``--unexport`` causes the specified shell variable to NOT be exported to child processes

- ``--path`` causes the specified variable to be treated as a path variable, meaning it will automatically be split on colons,  and joined using colons when quoted (`echo "$PATH"`) or exported.

- ``--unpath`` causes the specified variable to not be treated as a path variable. Variables with a name ending in "PATH" are automatically path variables, so this can be used to treat such a variable normally.

The following options are available:

- ``-e`` or ``--erase`` causes the specified shell variable to be erased

- ``-q`` or ``--query`` test if the specified variable names are defined. Does not output anything, but the builtins exit status is the number of variables specified that were not defined.

- ``-n`` or ``--names`` List only the names of all defined variables, not their value. The names are guaranteed to be sorted.

- ``-S`` or ``--show`` Shows information about the given variables. If no variable names are given then all variables are shown in sorted order. No other flags can be used with this option. The information shown includes whether or not it is set in each of the local, global, and universal scopes. If it is set in one of those scopes whether or not it is exported is reported. The individual elements are also shown along with the length of each element.

- ``-L`` or ``--long`` do not abbreviate long values when printing set variables


If a variable is set to more than one value, the variable will be a list with the specified elements. If a variable is set to zero elements, it will become a list with zero elements.

If the variable name is one or more list elements, such as ``PATH[1 3 7]``, only those list elements specified will be changed. If you specify a negative index when expanding or assigning to a list variable, the index will be calculated from the end of the list. For example, the index -1 means the last index of a list.

The scoping rules when creating or updating a variable are:

- Variables may be explicitly set to universal, global or local. Variables with the same name in different scopes will not be changed.

- If a variable is not explicitly set to be either universal, global or local, but has been previously defined, the previous variable scope is used.

- If a variable is not explicitly set to be either universal, global or local and has never before been defined, the variable will be local to the currently executing function. Note that this is different from using the ``-l`` or ``--local`` flag. If one of those flags is used, the variable will be local to the most inner currently executing block, while without these the variable will be local to the function. If no function is executing, the variable will be global.


The exporting rules when creating or updating a variable are identical to the scoping rules for variables:

- Variables may be explicitly set to either exported or not exported. When an exported variable goes out of scope, it is unexported.

- If a variable is not explicitly set to be exported or not exported, but has been previously defined, the previous exporting rule for the variable is kept.

- If a variable is not explicitly set to be either exported or unexported and has never before been defined, the variable will not be exported.


In query mode, the scope to be examined can be specified.

In erase mode, if variable indices are specified, only the specified slices of the list variable will be erased.

``set`` requires all options to come before any other arguments. For example, ``set flags -l`` will have the effect of setting the value of the variable ``flags`` to '-l', not making the variable local.

In assignment mode, ``set`` does not modify the exit status. This allows simultaneous capture of the output and exit status of a subcommand, e.g. ``if set output (command)``. In query mode, the exit status is the number of variables that were not found. In erase mode, ``set`` exits with a zero exit status in case of success, with a non-zero exit status if the commandline was invalid, if the variable was write-protected or if the variable did not exist.


Examples
--------


::

    # Prints all global, exported variables.
    set -xg

    # Sets the value of the variable $foo to be 'hi'.
    set foo hi

    # Appends the value "there" to the variable $foo.
    set -a foo there

    # Does the same thing as the previous two commands the way it would be done pre-fish 3.0.
    set foo hi
    set foo $foo there

    # Removes the variable $smurf
    set -e smurf

    # Changes the fourth element of the $PATH list to ~/bin
    set PATH[4] ~/bin

    # Outputs the path to Python if ``type -p`` returns true.
    if set python_path (type -p python)
        echo "Python is at $python_path"
    end

    # Like other shells, fish 3.1 supports this syntax for passing a variable to just one command:
    # Run fish with a temporary home directory.
    HOME=(mktemp -d) fish
    # Which is essentially the same as:
    begin; set -lx HOME (mktemp -d); fish; end

Notes
-----

Fish versions prior to 3.0 supported the syntax ``set PATH[1] PATH[4] /bin /sbin``, which worked like
``set PATH[1 4] /bin /sbin``. This syntax was not widely used, and was ambiguous and inconsistent.
