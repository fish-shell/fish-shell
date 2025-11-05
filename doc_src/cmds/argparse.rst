argparse - parse options passed to a fish script or function
============================================================

Synopsis
--------

.. synopsis::

    argparse [OPTIONS] OPTION_SPEC ... -- [ARG ...]


Description
-----------

This command makes it easy for fish scripts and functions to handle arguments. You pass arguments that define the known options, followed by a literal **--**, then the arguments to be parsed (which might also include a literal **--**). ``argparse`` then sets variables to indicate the passed options with their values, sets ``$argv_opts`` to the options and their values, and sets ``$argv`` to the remaining arguments. See the :ref:`usage <cmd-argparse-usage>` section below.

Each option specification (``OPTION_SPEC``) is written in the :ref:`domain specific language <cmd-argparse-option-specification>` described below. All OPTION_SPECs must appear after any argparse flags and before the ``--`` that separates them from the arguments to be parsed.

Each option that is seen in the ARG list will result in variables named ``_flag_X``, where **X** is the short flag letter and the long flag name (if they are defined). For example a **--help** option could cause argparse to define one variable called ``_flag_h`` and another called ``_flag_help``.

The variables will be set with local scope (i.e., as if the script had done ``set -l _flag_X``). If the flag is a boolean (that is, it is passed or not, it doesn't have a value) the values are the short and long flags seen. If the option is not a boolean the values will be zero or more values corresponding to the values collected when the ARG list is processed. If the flag was not seen the flag variable will not be set.

Options
-------

The following ``argparse`` options are available. They must appear before all *OPTION_SPEC*\ s:

**-n** or **--name** *NAME*
    Use *NAME* in error messages. By default the current function name will be used, or ``argparse`` if run outside of a function.

**-x** or **--exclusive** *OPTIONS*
    A comma separated list of options that are mutually exclusive. You can use this more than once to define multiple sets of mutually exclusive options.
    You give either the short or long version of each option, and you still need to otherwise define the options.

**-N** or **--min-args** *NUMBER*
    The minimum number of acceptable non-option arguments. The default is zero.

**-X** or **--max-args** *NUMBER*
    The maximum number of acceptable non-option arguments. The default is infinity.

**-u** or **--move-unknown**
    Allow unknown options, and move them from ``$argv`` to ``$argv_opts``. By default, Unknown options are treated as if they take optional arguments (i.e. have option spec ``=?``).

    The above means that if a group of short options contains an unknown short option *followed* by a known short option, the known short option is
    treated as an argument to the unknown one (e.g. ``--move-unknown h -- -oh`` will treat ``h`` as the argument to ``-o``, and so ``_flag_h`` will *not* be set).
    In contrast, if the known option comes first (and does not take any arguments), the known option will be recognised (e.g. ``argparse --move-unknown h -- -ho`` *will* set ``$_flag_h`` to ``-h``)

**-i** or **--ignore-unknown**
    Deprecated. This is like **--move-unknown**, except that unknown options and their arguments are kept in ``$argv`` and not moved to ``$argv_opts``. Unlike **--move-unknown**, this option makes it impossible to distinguish between an unknown option and non-option argument that starts with a ``-`` (since any ``--`` seperator in ``$argv`` will be removed).

**-S** or **--strict-longopts**
    This makes the parsing of long options more strict. In particular, *without* this flag, if ``long`` is a known long option flag, ``--long`` and ``--long=<value>`` can be abbreviated as:

    - ``-long`` and ``-long=<value>``, but *only* if there is no short flag ``l``.

    - ``--lo`` and ``--lo=<value>``, but *only* if there is no other long flag that starts with ``lo``. Similarly with any other non-empty prefix of ``long``.

    - ``-lo`` and ``-lo=<value>`` (i.e. combining the above two).

    With the ``--strict-longopts`` flag, the above three are parse errors: one must use the syntax ``--long`` or ``--long=<value>`` to use a long option called ``long``.

    This flag has no effect on the parsing of unknown options (which are parsed as if this flag is on).

    This option may be on all the time in the future, so do not rely on the behaviour without it.

**--unknown-arguments** *KIND*
    This option implies **--move-unknown**, unless **--ignore-unknown** is also given.
    This will modify the parsing behaviour of unknown options depending on the value of *KIND*:

        - **optional** (the default), allows each unknown option to take an optional argument (i.e. as if it had ``=?`` or ``=*`` in its option specification). For example, ``argparse --ignore-unknown --unknown-arguments=optional ab -- -u -a -ub`` will set ``_flag_a`` but *not* ``_flag_b``, as the ``b`` is treated as an argument to the second use of ``-u``.

        - **required** requires each unknown option to take an argument (i.e. as if it had ``=`` or ``=+`` in its option specification). If the above example was changed to use ``--unknown-arguments=required``, *neither* ``_flag_a`` nor ``_flag_b`` would be set: the ``-a`` will be treated as an argument to the first use of ``-u``, and the ``b`` as an argument to the second.

        - **none** forbids each unknown option from taking an argument (i.e. as if it had no ``=`` in its option specification). If the above example was changed to use ``--unknown-arguments=none``, *both* ``_flag_a`` and ``_flag_b`` would be set, as neither use of ``-u`` will be passed as taking an argument.

        Note that the above assumes that unknown long flags use the ``--`` "GNU-style" (e.g. if *KIND* is ``none``, and there is no ``bar`` long option, ``-bar`` is interpreted as three short flags, ``b``, ``a``, and ``r``; but if ``bar`` is known, ``-bar`` is treated the same as ``--bar``).

        When using ``--unknown-arguments=required``, you will get an error if the provided arguments end in an unknown option, since it has no argument. Similarly, with ``--unknown-arguments=none``, you will get an error if you use the ``--flag=value`` syntax and ``flag`` is an unknown option.

**-s** or **--stop-nonopt**
    Causes scanning the arguments to stop as soon as the first non-option argument is seen. Among other things, this is useful to implement subcommands that have their own options.

**-h** or **--help**
    Displays help about using this command.

.. _cmd-argparse-usage:

Usage
-----

To use this command, pass the option specifications (**OPTION_SPEC**), a mandatory **--**, and then the arguments to be parsed.

A simple example::

    argparse 'h/help' 'n/name=' -- $argv
    or return

If ``$argv`` is empty then there is nothing to parse and ``argparse`` returns zero to indicate success. If ``$argv`` is not empty then it is checked for flags ``-h``, ``--help``, ``-n`` and ``--name``. If they are found they are removed from the arguments and local variables called ``_flag_OPTION`` are set so the script can determine which options were seen. If ``$argv`` doesn't have any errors, like an unknown option or a missing mandatory value for an option, then ``argparse`` exits with a status of zero. Otherwise it writes appropriate error messages to stderr and exits with a status of one.

The ``or return`` means that the function returns ``argparse``'s status if it failed, so if it goes on ``argparse`` succeeded.

To use the flags argparse has extracted::

    # Checking for _flag_h and _flag_help is equivalent
    # We check if it has been given at least once
    if set -ql _flag_h
        echo "Usage: my_function [-h | --help] [-n | --name=NAME]" >&2
        return 1
    end

    set -l myname somedefault
    set -ql _flag_name[1]
    and set myname $_flag_name[-1] # here we use the *last* --name=

Any characters in the flag name that are not valid in a variable name (like ``-`` dashes) will be replaced with underscores.

The ``--`` argument is required. You do not have to include any option specifications or arguments after the ``--`` but you must include the ``--``. For example, this is acceptable::

    set -l argv foo
    argparse 'h/help' 'n/name' -- $argv
    argparse --min-args=1 -- $argv

But this is not::

    set -l argv
    argparse 'h/help' 'n/name' $argv

The first ``--`` seen is what allows the ``argparse`` command to reliably separate the option specifications and options to ``argparse`` itself (like ``--move-unknown``) from the command arguments, so it is required.

.. _cmd-argparse-option-specification:

Option Specifications
---------------------

Each option specification consists of:

- An optional alphanumeric short flag character.

- An optional long flag name preceded by a ``/``. If neither a short flag nor long flag are present, an error is reported.

    - If there is no short flag, and the long flag name is more than one character, the ``/`` can be omitted.

    - For backwards compatibility, if there is a short and a long flag, a ``-`` can be used in place of the ``/``, if the short flag is not to be usable by users (in which case it will also not be exposed as a flag variable).

- Nothing if the flag is a boolean that takes no argument or is an integer flag, or

    - **=** if it requires a value and only the last instance of the flag is saved, or

    - **=?** if it takes an optional value and only the last instance of the flag is saved, or

    - **=+** if it requires a value and each instance of the flag is saved, or

    - **=\*** if it takes an optional value *and* each instance of the flag is saved, storing the empty string when the flag was not given a value.

- Optionally a ``&``, indicating that the option and any attached values are not to be saved in ``$argv`` or ``$argv_opts``. This does not affect the the ``_flag_`` variables.

- Nothing if the flag is a boolean that takes no argument, or

    - ``!`` followed by fish script to validate the value. Typically this will be a function to run. If the exit status is zero the value for the flag is valid. If non-zero the value is invalid. Any error messages should be written to stdout (not stderr). See the section on :ref:`Flag Value Validation <flag-value-validation>` for more information.

See the :doc:`fish_opt <fish_opt>` command for a friendlier but more verbose way to create option specifications.

If a flag is not seen when parsing the arguments then the corresponding _flag_X var(s) will not be set.

Integer flag
------------

Sometimes commands take numbers directly as options, like ``foo -55``. To allow this one option spec can have the ``#`` modifier so that any integer will be understood as this flag, and the last number will be given as its value (as if ``=`` was used).

The ``#`` must follow the short flag letter (if any), and other modifiers like ``=`` are not allowed, except for ``-`` (for backwards compatibility)::

  m#maximum

This does not read numbers given as ``+NNN``, only those that look like flags - ``-NNN``.

Note: Optional arguments
------------------------

An option defined with ``=?`` or ``=*`` can take optional arguments. Optional arguments have to be *directly attached* to the option they belong to.

That means the argument will only be used for the option if you use it like::

  cmd --flag=value
  # or
  cmd  -fvalue

but not if used like::

  cmd --flag value
  # "value" here will be used as a positional argument
  # and "--flag" won't have an argument.

If this weren't the case, using an option without an optional argument would be difficult if you also wanted to use positional arguments.

For example::

  grep --color auto
  # Here "auto" will be used as the search string,
  # "color" will not have an argument and will fall back to the default,
  # which also *happens to be* auto.
  grep --color always
  # Here grep will still only use color "auto"matically
  # and search for the string "always".

This isn't specific to argparse but common to all things using ``getopt(3)`` (if they have optional arguments at all). That ``grep`` example is how GNU grep actually behaves.

.. _flag-value-validation:

Flag Value Validation
---------------------

Sometimes you need to validate the option values. For example, that it is a valid integer within a specific range, or an ip address, or something entirely different. You can always do this after ``argparse`` returns but you can also request that ``argparse`` perform the validation by executing arbitrary fish script. To do so append an ``!`` (exclamation-mark) then the fish script to be run. When that code is executed three vars will be defined:

- ``_argparse_cmd`` will be set to the value of the value of the ``argparse --name`` value.

- ``_flag_name`` will be set to the short or long flag that being processed.

- ``_flag_value`` will be set to the value associated with the flag being processed.

These variables are passed to the function as local exported variables.

The script should write any error messages to stdout, not stderr. It should return a status of zero if the flag value is valid otherwise a non-zero status to indicate it is invalid.

Fish ships with a ``_validate_int`` function that accepts a ``--min`` and ``--max`` flag. Let's say your command accepts a ``-m`` or ``--max`` flag and the minimum allowable value is zero and the maximum is 5. You would define the option like this: ``m/max=!_validate_int --min 0 --max 5``. The default if you call ``_validate_int`` without those flags is to check that the value is a valid integer with no limits on the min or max value allowed.

Here are some examples of flag validations::

  # validate that a path is a directory
  argparse 'p/path=!test -d "$_flag_value"' -- --path $__fish_config_dir
  # validate that a function does not exist
  argparse 'f/func=!not functions -q "$_flag_value"' -- -f alias
  # validate that a string matches a regex
  argparse 'c/color=!string match -rq \'^#?[0-9a-fA-F]{6}$\' "$_flag_value"' -- -c 'c0ffee'
  # validate with a validator function
  argparse 'n/num=!_validate_int --min 0 --max 99' -- --num 42

Example OPTION_SPECs
--------------------

Some *OPTION_SPEC* examples:

- ``h/help`` means that both ``-h`` and ``--help`` are valid. The flag is a boolean and can be used more than once. If either flag is used then ``_flag_h`` and ``_flag_help`` will be set to however either flag was seen, as many times as it was seen. So it could be set to ``-h``, ``-h`` and ``--help``, and ``count $_flag_h`` would yield "3".

- ``help`` means that only ``--help`` is valid. The flag is a boolean and can be used more than once. If it is used then ``_flag_help`` will be set as above. Also ``h-help`` (with an arbitrary short letter) for backwards compatibility.

- ``help&`` is similar (it will *remove* ``--help`` from ``$argv``), the difference is that ``--help``` will *not* placed in ``$argv_opts``.

- ``longonly=`` is a flag ``--longonly`` that requires an option, there is no short flag or even short flag variable.

- ``n/name=`` means that both ``-n`` and ``--name`` are valid. It requires a value and can be used at most once. If the flag is seen then ``_flag_n`` and ``_flag_name`` will be set with the single mandatory value associated with the flag.

- ``n/name=?`` means that both ``-n`` and ``--name`` are valid. It accepts an optional value and can be used at most once. If the flag is seen then ``_flag_n`` and ``_flag_name`` will be set with the value associated with the flag if one was provided else it will be set with no values.

- ``n/name=*`` is similar, but the flag can be used more than once. If the flag is seen then ``_flag_n`` and ``_flag_name`` will be set with the values associated with each occurence. Each value will be the value given to the option, or the empty string if no value was given.

- ``name=+`` means that only ``--name`` is valid. It requires a value and can be used more than once. If the flag is seen then ``_flag_name`` will be set with the values associated with each occurrence.

- ``x`` means that only ``-x`` is valid. It is a boolean that can be used more than once. If it is seen then ``_flag_x`` will be set as above.

- ``/x`` is similar, but only ``--x`` is valid (instead of ``-x``).

- ``x=``, ``x=?``, and ``x=+`` are similar to the n/name examples above but there is no long flag alternative to the short flag ``-x``.

- ``#max`` (or ``#-max``) means that flags matching the regex "^--?\\d+$" are valid. When seen they are assigned to the variable ``_flag_max``. This allows any valid positive or negative integer to be specified by prefixing it with a single "-". Many commands support this idiom. For example ``head -3 /a/file`` to emit only the first three lines of /a/file.

- ``n#max`` means that flags matching the regex "^--?\\d+$" are valid. When seen they are assigned to the variables ``_flag_n`` and ``_flag_max``. This allows any valid positive or negative integer to be specified by prefixing it with a single "-". Many commands support this idiom. For example ``head -3 /a/file`` to emit only the first three lines of /a/file. You can also specify the value using either flag: ``-n NNN`` or ``--max NNN`` in this example.

- ``#longonly`` causes the last integer option to be stored in ``_flag_longonly``.

After parsing the arguments the ``argv`` variable is set with local scope to any values not already consumed during flag processing. If there are no unbound values the variable is set but ``count $argv`` will be zero. Similarly, the ``argv_opts`` variable is set with local scope to the arguments that *were* consumed during flag processing. This allows forwarding ``$argv_opts`` to another command, together with additional arguments.

If an error occurs during argparse processing it will exit with a non-zero status and print error messages to stderr.

Examples
---------

A simple use::

    argparse h/help -- $argv
    or return

    if set -q _flag_help
        # TODO: Print help here
        return 0
    end

This supports one option - ``-h`` / ``--help``. Any other option is an error. If it is given it prints help and exits.

How :doc:`fish_add_path` parses its args::

  argparse -x g,U -x P,U -x a,p g/global U/universal P/path p/prepend a/append h/help m/move v/verbose n/dry-run -- $argv

There are a variety of boolean flags, all with long and short versions. A few of these cannot be used together, and that is what the ``-x`` flag is used for.
``-x g,U`` means that ``--global`` and ``--universal`` or their short equivalents conflict, and if they are used together you get an error.
In this case you only need to give the short or long flag, not the full option specification.

After this it figures out which variable it should operate on according to the ``--path`` flag::

    set -l var fish_user_paths
    set -q _flag_path
    and set var PATH

    # ...

    # Check for --dry-run.
    # The "-" has been replaced with a "_" because
    # it is not valid in a variable name
    not set -ql _flag_dry_run
    and set $var $result


An example of using ``$argv_opts`` to forward known options to another command, whilst adding new options::

    function my-head
        # The following option is the only existing one to head that takes arguments
        # (we will forward it verbatim).
        set -l opt_spec n/lines=
        # --qwords is a new option, but --bytes is an existing one which we will modify below
        set -a opt_spec "qwords=&" "c/bytes=&"
        argparse --strict-longopts --move-unknown --unknown-arguments=none $opt_spec -- $argv || return
        if set -q _flag_qwords
            # --qwords allows specifying the size in multiples of 8 bytes
            set -a argv_opts --bytes=(math -- $_flag_qwords \* 8 || return)
        else if set -q _flag_bytes
            # Allows using a 'q' suffix, e.g. --bytes=4q to mean 4*8 bytes.
            if string match -qr 'q$' -- $_flag_bytes
                set -a argv_opts --bytes=(math -- (string replace -r 'q$' '*8' -- $_flag_bytes) || return)
            else
                # Keep the users setting
                set -a argv_opts --bytes=$_flag_bytes
            end

        end

        if test (count $argv) -eq 0
            # Default to heading /dev/kmsg (whereas head defaults to stdin)
            set -l argv /dev/kmsg
        end

        # Call the real head with our modified options and arguments.
        head $argv_opts -- $argv
    end


The argparse call above saves all the options we do *not* want to process in ``$argv_opts``. (The ``--qwords`` and ``--bytes`` options are *not* saved there as their option spec's end in a ``~``). The code then processes the ``--qwords`` and ``--bytess`` options using the the ``$_flag_OPTION`` variables, and puts the transformed options in ``$argv_opts`` (which already contains all the original options, *other* than ``--qwords`` and ``--bytes``).

Note that because the ``argparse`` call above uses ``--move-unknown`` and ``--unknown-arguments=none``, we only need to tell it the arguments to ``head`` that take a value. This allows the wrapper script to accurately work out the *non*-option arguments (i.e. ``$argv``, the filenames that ``head`` is to operate on). Using ``--unknown-arguments=optional`` and explicitly listing all the known options to ``head`` however would have the advantage that if ``head`` were to add new options, they could still be used with the wrapper script using the "stuck" form for arguments (e.g. ``-o<arg>``, or ``--opt=<arg>``).

Note that the ``--strict-longopts`` is required to be able to correctly pass short options, e.g. without it ``my-head -q --bytes 10q``, will actually parse the ``-q`` as shorthand for ``--qwords``.
