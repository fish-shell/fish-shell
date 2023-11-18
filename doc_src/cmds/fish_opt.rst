.. _cmd-fish_opt:

fish_opt - create an option specification for the argparse command
==================================================================

Synopsis
--------

.. synopsis::

    fish_opt [(-slor | --multiple-vals=) OPTNAME]
    fish_opt --help

Description
-----------

This command provides a way to produce option specifications suitable for use with the :doc:`argparse <argparse>` command. You can, of course, write the option specifications by hand without using this command. But you might prefer to use this for the clarity it provides.

The following ``argparse`` options are available:

**-s** or **--short**
    Takes a single letter that is used as the short flag in the option being defined. This option is mandatory.

**-l** or **--long**
    Takes a string that is used as the long flag in the option being defined. This option is optional and has no default. If no long flag is defined then only the short flag will be allowed when parsing arguments using the option specification.

**--long-only**
    The option being defined will only allow the long flag name to be used. The short flag name must still be defined (i.e., **--short** must be specified) but it cannot be used when parsing arguments using this option specification.

**-o** or **--optional-val**
    The option being defined can take a value, but it is optional rather than required. If the option is seen more than once when parsing arguments, only the last value seen is saved. This means the resulting flag variable created by ``argparse`` will zero elements if no value was given with the option else it will have exactly one element.

**-r** or **--required-val**
    The option being defined requires a value. If the option is seen more than once when parsing arguments, only the last value seen is saved. This means the resulting flag variable created by ``argparse`` will have exactly one element.

**--multiple-vals**
    The option being defined requires a value each time it is seen. Each instance is stored. This means the resulting flag variable created by ``argparse`` will have one element for each instance of this option in the arguments.

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
    set options $options (fish_opt -s m -l max --required-val)
    argparse $options -- $argv


Same as above but with a third flag that can be given multiple times saving the value of each instance seen and only the long flag name (``--token``) can be used:



::

    set -l options (fish_opt --short=h --long=help)
    set options $options (fish_opt --short=m --long=max --required-val)
    set options $options (fish_opt --short=t --long=token --multiple-vals --long-only)
    argparse $options -- $argv

