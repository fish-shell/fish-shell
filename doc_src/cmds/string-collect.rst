string-collect - join strings into one
======================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string collect [(-N | --no-trim-newlines)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string collect`` collects its input into a single output argument, without splitting the output when used in a command substitution. This is useful when trying to collect multiline output from another command into a variable. Exit status: 0 if any output argument is non-empty, or 1 otherwise.

If invoked with multiple arguments instead of input, ``string collect`` preserves each argument separately, where the number of output arguments is equal to the number of arguments given to ``string collect``.

Any trailing newlines on the input are trimmed, just as with ``"$(cmd)"`` substitution in sh. ``--no-trim-newlines`` can be used to disable this behavior, which may be useful when running a command such as ``set contents (cat filename | string collect -N)``.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ echo \"(echo one\ntwo\nthree | string collect)\"
    "one
    two
    three
    "

    >_ echo \"(echo one\ntwo\nthree | string collect -N)\"
    "one
    two
    three"

.. END EXAMPLES
