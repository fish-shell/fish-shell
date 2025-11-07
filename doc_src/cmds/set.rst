set - display and change shell variables
========================================

Synopsis
--------

.. synopsis::

    set
    set (-f | --function) (-l | --local) (-g | --global) (-U | --universal) [--no-event]
    set [-Uflg] NAME [VALUE ...]
    set [-Uflg] NAME[[INDEX ...]] [VALUE ...]
    set (-x | --export) (-u | --unexport) [-Uflg] NAME [VALUE ...]
    set (-a | --append) (-p | --prepend) [-Uflg] NAME VALUE ...
    set (-q | --query) (-e | --erase) [-Uflg] [NAME][[INDEX]] ...]
    set (-S | --show) (-L | --long) [NAME ...]

Description
-----------

``set`` manipulates :ref:`shell variables <variables>`.

If both *NAME* and *VALUE* are provided, ``set`` assigns any values to variable *NAME*.
Variables in fish are :ref:`lists <variables-lists>`, multiple values are allowed.
One or more variable *INDEX* can be specified including ranges (not for all options.)

If no *VALUE* is given, the variable will be set to the empty list.

If ``set`` is ran without arguments, it prints the names and values of all shell variables in sorted order.
Passing :ref:`scope <variables-scope>` or :ref:`export <variables-export>` flags allows filtering this to only matching variables, so ``set --local`` would only show local variables.

With ``--erase`` and optionally a scope flag ``set`` will erase the matching variable (or the variable of that name in the smallest possible scope).

With ``--show``, ``set`` will describe the given variable names, explaining how they have been defined - in which scope with which values and options.

The following options control variable scope:

**-U** or **--universal**
    Sets a universal variable.
    The variable will be immediately available to all the user's ``fish`` instances on the machine, and will be persisted across restarts of the shell.

**-f** or **--function**
    Sets a variable scoped to the executing function.
    It is erased when the function ends.

**-l** or **--local**
    Sets a locally-scoped variable in this block.
    It is erased when the block ends.
    Outside of a block, this is the same as **--function**.

**-g** or **--global**
    Sets a globally-scoped variable.
    Global variables are available to all functions running in the same shell.
    They can be modified or erased.

These options modify how variables operate:

**--export** or **-x**
    Causes the specified shell variable to be exported to child processes (making it an "environment variable").

**--unexport** or **-u**
    Causes the specified shell variable to NOT be exported to child processes.

**--path**
    Treat specified variable as a :ref:`path variable <variables-path>`; variable will be split on colons (``:``) and will be displayed joined by colons when quoted (``echo "$PATH"``) or exported.

**--unpath**
     Causes variable to no longer be treated as a :ref:`path variable <variables-path>`.
     Note: variables ending in "PATH" are automatically path variables.

Further options:

**-a** or **--append** *NAME* *VALUE* ...
    Appends *VALUES* to the current set of values for variable **NAME**.
    Can be used with **--prepend** to both append and prepend at the same time.
    This cannot be used when assigning to a variable slice.

**-p** or **--prepend** *NAME* *VALUE* ...
    Prepends *VALUES* to the current set of values for variable **NAME**.
    This can be used with **--append** to both append and prepend at the same time.
    This cannot be used when assigning to a variable slice.

**-e** or **--erase** *NAME*\[*INDEX*\]
    Causes the specified shell variables to be erased.
    Supports erasing from multiple scopes at once.
    Individual items in a variable at *INDEX* in brackets can be specified.

**-q** or **--query** *NAME*\[*INDEX*\]
    Test if the specified variable names are defined.
    If an *INDEX* is provided, check for items at that slot.
    Does not output anything, but the shell status is set to the number of variables specified that were not defined, up to a maximum of 255.
    If no variable was given, it also returns 255.

**-n** or **--names**
    List only the names of all defined variables, not their value.
    The names are guaranteed to be sorted.

**-S** or **--show**
    Shows information about the given variables.
    If no variable names are given then all variables are shown in sorted order.
    It shows the scopes the given variables are set in, along with the values in each and whether or not it is exported.
    No other flags can be used with this option.

**--no-event**
    Don't generate a variable change event when setting or erasing a variable.
    We recommend using this carefully because the event handlers are usually set up for a reason.
    Possible uses include modifying the variable inside a variable handler.

**-L** or **--long**
    Do not abbreviate long values when printing set variables.

**-h** or **--help**
    Displays help about using this command.

If a variable is set to more than one value, the variable will be a list with the specified elements.
If a variable is set to zero elements, it will become a list with zero elements.

If the variable name is one or more list elements, such as ``PATH[1 3 7]``, only those list elements specified will be changed.
If you specify a negative index when expanding or assigning to a list variable, the index will be calculated from the end of the list.
For example, the index -1 means the last index of a list.

The scoping rules when creating or updating a variable are:

- Variables may be explicitly set as universal, global, function, or local.
  Variables with the same name but in a different scope will not be changed.

- If the scope of a variable is not explicitly set *but a variable by that name has been previously defined*, the scope of the existing variable is used.
  If the variable is already defined in multiple scopes, the variable with the narrowest scope will be updated.

- If a variable's scope is not explicitly set and there is no existing variable by that name, the variable will be local to the currently executing function.
  Note that this is different from using the ``-l`` or ``--local`` flag, in which case the variable will be local to the most-inner currently executing block, while without them the variable will be local to the function as a whole.
  If no function is executing, the variable will be set in the global scope.


The exporting rules when creating or updating a variable are identical to the scoping rules for variables:

- Variables may be explicitly set to either exported or not exported.
  When an exported variable goes out of scope, it is unexported.

- If a variable is not explicitly set to be exported or not exported, but has been previously defined, the previous exporting rule for the variable is kept.

- If a variable is not explicitly set to be either exported or unexported and has never before been defined, the variable will not be exported.

In query mode, the scope to be examined can be specified.
Whether the variable has to be a path variable or exported can also be specified.

In erase mode, if variable indices are specified, only the specified slices of the list variable will be erased.

``set`` requires all options to come before any other arguments.
For example, ``set flags -l`` will have the effect of setting the value of the variable :envvar:`flags` to '-l', not making the variable local.

Exit status
-----------

In assignment mode, ``set`` does not modify the exit status, but passes along whatever :envvar:`status` was set, including by command substitutions.
This allows capturing the output and exit status of a subcommand, like in ``if set output (command)``.

In query mode, the exit status is the number of variables that were not found.

In erase mode, ``set`` exits with a zero exit status in case of success, with a non-zero exit status if the commandline was invalid, if any of the variables did not exist or was a :ref:`special read-only variable <variables-special>`.


Examples
--------

Print all global, exported variables::

    > set -gx

Set the value of the variable _$foo_ to be 'hi'.::

    > set foo hi

Append the value "there" to the variable $foo::

    > set -a foo there

Remove _$smurf_ from the scope::

    > set -e smurf

Remove _$smurf_ from the global and universal scopes::

    > set -e -Ug smurf

Change the fourth element of the $PATH list to ~/bin::

    > set PATH[4] ~/bin

Outputs the path to Python if ``type -p`` returns true::

    if set python_path (type -p python)
        echo "Python is at $python_path"
    end

Setting a variable doesn't modify $status; a command substitution still will, though::

    > echo $status
    0
    > false
    > set foo bar
    > echo $status
    1
    > true
    > set foo banana (false)
    > echo $status
    1

``VAR=VALUE command`` sets a variable for just one command, like other shells.
This runs fish with a temporary home directory::

    > HOME=(mktemp -d) fish

(which is essentially the same as)::

    > begin; set -lx HOME (mktemp -d); fish; end

Notes
-----
- Fish versions prior to 3.0 supported the syntax ``set PATH[1] PATH[4] /bin /sbin``, which worked like ``set PATH[1 4] /bin /sbin``.
