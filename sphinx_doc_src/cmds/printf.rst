.. _cmd-printf:

printf - display text according to a format string
==================================================

Synopsis
--------

::

    printf FORMAT [ARGUMENT ...]

Description
-----------
printf formats the string FORMAT with ARGUMENT, and displays the result.

The string FORMAT should contain format specifiers, each of which are replaced with successive arguments according to the specifier. Specifiers are detailed below, and are taken from the C library function ``printf(3)``.

Unlike :ref:`echo <cmd-echo>`, ``printf`` does not append a new line unless it is specified as part of the string.

Valid format specifiers are:

- ``%d``: Argument will be used as decimal integer (signed or unsigned)

- ``%i``: Argument will be used as a signed integer

- ``%o``: An octal unsigned integer

- ``%u``: An unsigned decimal integer

- ``%x`` or ``%X``: An unsigned hexadecimal integer

- ``%f``, ``%g`` or ``%G``: A floating-point number

- ``%e`` or ``%E``: A floating-point number in scientific (XXXeYY) notation

- ``%s``: A string

- ``%b``: As a string, interpreting backslash escapes, except that octal escapes are of the form \0 or \0ooo.

``%%`` signifies a literal "%".

Note that conversion may fail, e.g. "102.234" will not losslessly convert to an integer, causing printf to print an error.

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

The ``format`` argument is re-used as many times as necessary to convert all of the given arguments. If a format specifier is not appropriate for the given argument, an error is printed. For example, ``printf '%d' "102.234"`` produces an error, as "102.234" cannot be formatted as an integer.

This file has been imported from the printf in GNU Coreutils version 6.9. If you would like to use a newer version of printf, for example the one shipped with your OS, try ``command printf``.

Example
-------



::

    printf '%s\\t%s\\n' flounder fish

Will print "flounder	fish" (separated with a tab character), followed by a newline character. This is useful for writing completions, as fish expects completion scripts to output the option followed by the description, separated with a tab character.



::

    printf '%s: %d' "Number of bananas in my pocket" 42

Will print "Number of bananas in my pocket: 42", _without_ a newline.
