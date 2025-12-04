fish_opt - create an option specification for the argparse command
==================================================================

Synopsis
--------

.. synopsis::

    fish_opt [-s ALPHANUM] [-l LONG-NAME] [-ormd] [--long-only] [-v COMMAND OPTIONS ... ]
    fish_opt --help

Description
-----------

This command provides a way to produce option specifications suitable for use with the :doc:`argparse <argparse>` command. You can, of course, write the option specifications by hand without using this command. But you might prefer to use this for the clarity it provides.

The following ``argparse`` options are available:

**-s** or **--short** *ALPHANUM*
    Takes a single letter or number that is used as the short flag in the option being defined. Either this option or the **--long** option must be provided.

**-l** or **--long** *LONG-NAME*
    Takes a string that is used as the long flag in the option being defined. This option is optional and has no default. If no long flag is defined then only the short flag will be allowed when parsing arguments using the option specification.

**--long-only**
    Deprecated. The option being defined will only allow the long flag name to be used, even if the short flag is defined (i.e., **--short** is specified).

**-o** or **--optional-val**
    The option being defined can take a value, but it is optional rather than required. If the option is seen more than once when parsing arguments, only the last value seen is saved. This means the resulting flag variable created by ``argparse`` will zero elements if no value was given with the option else it will have exactly one element.

**-r** or **--required-val**
    The option being defined requires a value. If the option is seen more than once when parsing arguments, only the last value seen is saved. This means the resulting flag variable created by ``argparse`` will have exactly one element.

**-m** or **--multiple-vals**
    The value of each instance of the option is accumulated. If **--optional-val** is provided, the value is optional, and an empty string is stored if no value is provided. Otherwise, the **--requiured-val** option is implied and each instance of the option requires a value. This means the resulting flag variable created by ``argparse`` will have one element for each instance of this option in the arguments, even for instances that did not provide a value.

**-d** or **--delete**
    The option and any values will be deleted from the ``$argv_opts`` variables set by ``argparse``
    (as with other options, it will also be deleted from ``$argv``).

**-v** or **--validate** *COMMAND* *OPTION...*
    This option must be the last one, and requires one of ``-o``, ``-r``, or ``-m``. All the remaining arguments are interpreted a fish script to run to validate the value of the argument, see ``argparse`` documentation for more details. Note that the interpretation of *COMMAND* *OPTION...* is similar to ``eval``, so you may need to quote or escape special characters *twice* if you want them to be interpreted literally when the validate script is run.

**-h** or **--help**
    Displays help about using this command.

Examples
--------

Define a single option specification for the boolean help flag:



::

    set -l options (fish_opt -s h -l help)
    argparse $options -- $argv


Same as above but with a second flag that requires a value:



::

    set -l options (fish_opt -s h -l help)
    set options $options (fish_opt -s m -l max -r)
    argparse $options -- $argv

Same as above but the value of the second flag cannot be the empty string:

::

    set -l options (fish_opt -s h -l help)
    set options $options (fish_opt -s m -l max -rv test \$_flag_value != "''")
    argparse $options -- $argv

Same as above but with a third flag that can be given multiple times saving the value of each instance seen and only a long flag name (``--token``) is defined:



::

    set -l options (fish_opt --short=h --long=help)
    set options $options (fish_opt --short=m --long=max --required-val --validate test \$_flag_value != "''")
    set options $options (fish_opt --long=token --multiple-vals)
    argparse $options -- $argv

