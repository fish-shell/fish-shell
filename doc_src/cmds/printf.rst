printf - display text according to a format string
==================================================

Synopsis
--------

.. synopsis::

    printf FORMAT [ARGUMENT ...]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``printf``.
          To see the documentation on any non-fish versions, use ``command man printf``.

``printf`` uses the format string *FORMAT* to print the *ARGUMENT* arguments. This means that it takes format specifiers in the format string and replaces each with an argument.

The *FORMAT* argument is re-used as many times as necessary to convert all of the given arguments. So ``printf %s\n flounder catfish clownfish shark`` will print four lines.

Unlike :doc:`echo <echo>`, ``printf`` does not append a new line unless it is specified as part of the string.

It doesn't support any options, so there is no need for a ``--`` separator, which makes it easier to use for arbitrary input than ``echo``. [#]_ 

Format Specifiers
-----------------
Valid format specifiers are taken from the C library function ``printf(3)``:

- ``%d`` or ``%i``: Argument will be used as decimal integer (signed or unsigned)

- ``%o``: An octal unsigned integer

- ``%u``: An unsigned decimal integer - this means negative numbers will wrap around

- ``%x`` or ``%X``: An unsigned hexadecimal integer

- ``%f``, ``%g`` or ``%G``: A floating-point number. ``%f`` defaults to 6 places after the decimal point (which is locale-dependent - e.g. in de_DE it will be a ``,``). ``%g`` and ``%G`` will trim trailing zeroes and switch to scientific notation (like ``%e``) if the numbers get small or large enough.

- ``%e`` or ``%E``: A floating-point number in scientific (XXXeYY) notation

- ``%s``: A string

- ``%b``: As a string, interpreting backslash escapes, except that octal escapes are of the form \0 or \0ooo.

``%%`` signifies a literal "%".

Conversion can fail, e.g. "102.234" can't losslessly convert to an integer, causing printf to print an error. If you are okay with losing information, silence errors with ``2>/dev/null``.

A number between the ``%`` and the format letter specifies the width. The result will be left-padded with spaces.

Backslash Escapes
-----------------
printf also knows a number of backslash escapes:

- ``\"`` double quote
- ``\\`` backslash
- ``\a`` alert (bell)
- ``\b`` backspace
- ``\c`` produce no further output
- ``\e`` escape
- ``\f`` form feed
- ``\n`` new line
- ``\r`` carriage return
- ``\t`` horizontal tab
- ``\v`` vertical tab
- ``\ooo`` octal number (ooo is 1 to 3 digits)
- ``\xhh`` hexadecimal number (hhh is 1 to 2 digits)
- ``\uhhhh`` 16-bit Unicode character (hhhh is 4 digits)
- ``\Uhhhhhhhh`` 32-bit Unicode character (hhhhhhhh is 8 digits)

Errors and Return Status
------------------------
If the given argument doesn't work for the given format (like when you try to convert a number like 3.141592 to an integer), printf prints an error, to stderr. printf will then also return non-zero, but will still try to print as much as it can.

It will also return non-zero if no argument at all was given, in which case it will print nothing.

This printf has been imported from the printf in GNU Coreutils version 6.9. If you would like to use a newer version of printf, for example the one shipped with your OS, try ``command printf``.

Example
-------

::

    printf '%s\t%s\n' flounder fish

Will print "flounder	fish" (separated with a tab character), followed by a newline character. This is useful for writing completions, as fish expects completion scripts to output the option followed by the description, separated with a tab character.

::

    printf '%s: %d' "Number of bananas in my pocket" 42

Will print "Number of bananas in my pocket: 42", `without` a newline.

See Also
--------

- the :doc:`echo <echo>` command, for simpler output

Footnotes
---------
.. [#] In fact, while fish's ``echo`` supports ``--``, POSIX forbids it, so other implementations can't be used if the input contains anything starting with ``-``.
