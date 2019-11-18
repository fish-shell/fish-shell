string-replace - replace substrings
===================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string replace [(-a | --all)] [(-f | --filter)] [(-i | --ignore-case)] [(-r | --regex)] [(-q | --quiet)] PATTERN REPLACEMENT [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string replace`` is similar to ``string match`` but replaces non-overlapping matching substrings with a replacement string and prints the result. By default, PATTERN is treated as a literal substring to be matched.

If ``-r`` or ``--regex`` is given, PATTERN is interpreted as a Perl-compatible regular expression, and REPLACEMENT can contain C-style escape sequences like ``\t`` as well as references to capturing groups by number or name as ``$n`` or ``${n}``.

If you specify the ``-f`` or ``--filter`` flag then each input string is printed only if a replacement was done. This is useful where you would otherwise use this idiom: ``a_cmd | string match pattern | string replace pattern new_pattern``. You can instead just write ``a_cmd | string replace --filter pattern new_pattern``.

Exit status: 0 if at least one replacement was performed, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

Replace Literal Examples
^^^^^^^^^^^^^^^^^^^^^^^^

::

    >_ string replace is was 'blue is my favorite'
    blue was my favorite

    >_ string replace 3rd last 1st 2nd 3rd
    1st
    2nd
    last

    >_ string replace -a ' ' _ 'spaces to underscores'
    spaces_to_underscores

Replace Regex Examples
^^^^^^^^^^^^^^^^^^^^^^

::

    >_ string replace -r -a '[^\\d.]+' ' ' '0 one two 3.14 four 5x'
    0 3.14 5

    >_ string replace -r '(\\w+)\\s+(\\w+)' '$2 $1 $$' 'left right'
    right left $

    >_ string replace -r '\\s*newline\\s*' '\\n' 'put a newline here'
    put a
    here

.. END EXAMPLES
