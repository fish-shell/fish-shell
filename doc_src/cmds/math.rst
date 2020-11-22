.. _cmd-math:

math - perform mathematics calculations
=======================================

Synopsis
--------

::

    math [-sN | --scale=N] [-bBASE | --base=BASE] [--] EXPRESSION


Description
-----------

``math`` is used to perform mathematical calculations. It supports all the usual operations such as addition, subtraction, etc. As well as functions like ``abs()``, ``sqrt()`` and ``log2()``.

By default, the output is as a float with trailing zeroes trimmed. To get a fixed representation, the ``--scale`` option can be used, including ``--scale=0`` for integer output.

Keep in mind that parameter expansion happens before expressions are evaluated. This can be very useful in order to perform calculations involving shell variables or the output of command substitutions, but it also means that parenthesis (``()``) and the asterisk (``*``) glob character have to be escaped or quoted. ``x`` can also be used to denote multiplication, but it needs to be followed by whitespace to distinguish it from hexadecimal numbers.

``math`` ignores whitespace between arguments and takes its input as multiple arguments (internally joined with a space), so ``math 2 +2`` and ``math "2 +    2"`` work the same. ``math 2 2`` is an error.

The following options are available:

- ``-sN`` or ``--scale=N`` sets the scale of the result. ``N`` must be an integer or the word "max" for the maximum scale. A scale of zero causes results to be rounded down to the nearest integer. So ``3/2`` returns ``1`` rather than ``2`` which ``1.5`` would normally round to. This is for compatibility with ``bc`` which was the basis for this command prior to fish 3.0.0. Scale values greater than zero causes the result to be rounded using the usual rules to the specified number of decimal places.

- ``-b BASE`` or ``--base BASE`` sets the numeric base used for output (``math`` always understands hexadecimal numbers as input). It currently understands "hex" or "16" for hexadecimal and "octal" or "8" for octal and implies a scale of 0 (other scales cause an error), so it will truncate the result down to an integer. This might change in the future. Hex numbers will be printed with a ``0x`` prefix. Octal numbers will have a prefix of ``0`` and aren't understood by ``math`` as input.

Return Values
-------------

If the expression is successfully evaluated and doesn't over/underflow or return NaN the return ``status`` is zero (success) else one.

Syntax
------

``math`` knows some operators, constants, functions and can (obviously) read numbers.

For numbers, ``.`` is always the radix character regardless of locale - ``2.5``, not ``2,5``. Scientific notation (``10e5``) and hexadecimal (``0xFF``) are also available.

Operators
---------

``math`` knows the following operators:

- ``+`` for addition and ``-`` for subtraction.

- ``*`` or ``x`` for multiplication, ``/`` for division. (Note that ``*`` is the glob character and needs to be quoted or escaped, ``x`` needs to be followed by whitespace or it looks like ``0x`` hexadecimal notation.)

- ``^`` for exponentiation.

- ``%`` for modulo.

- ``(`` and ``)`` for grouping. (These need to be quoted or escaped because ``()`` denotes a command substitution.)

They are all used in an infix manner - ``5 + 2``, not ``+ 5 2``.

Constants
---------

``math`` knows the following constants:

- ``e`` - Euler's number.
- ``pi`` - You know that one. Half of Tau. (``tau`` is also implemented)

Use them without a leading ``$`` - ``pi - 3`` should be about 0.

Functions
---------

``math`` supports the following functions:

- ``abs``
- ``acos``
- ``asin``
- ``atan``
- ``atan2``
- ``bitand``, ``bitor`` and ``bitxor`` to perform bitwise operations. These will throw away any non-integer parts and interpret the rest as an int.
- ``ceil``
- ``cos``
- ``cosh``
- ``exp`` - the base-e exponential function
- ``fac`` - factorial
- ``floor``
- ``ln``
- ``log`` or ``log10`` - the base-10 logarithm
- ``ncr``
- ``npr``
- ``pow(x,y)`` returns x to the y (and can be written as ``x ^ y``)
- ``round`` - rounds to the nearest integer, away from 0
- ``sin``
- ``sinh``
- ``sqrt``
- ``tan``
- ``tanh``

All of the trigonometric functions use radians.

Examples
--------

``math 1+1`` outputs 2.

``math $status - 128`` outputs the numerical exit status of the last command minus 128.

``math 10 / 6`` outputs ``1.666667``.

``math -s0 10.0 / 6.0`` outputs ``1``.

``math -s3 10 / 6`` outputs ``1.666``.

``math "sin(pi)"`` outputs ``0``.

``math 5 \* 2`` or ``math "5 * 2"`` or ``math 5 "*" 2`` all output ``10``.

``math 0xFF`` outputs 255, ``math 0 x 3`` outputs 0 (because it computes 0 multiplied by 3).

``math "bitand(0xFE, 0x2e)"`` outputs 46.

``math "bitor(9,2)"`` outputs 11.

``math --base=hex 192`` prints ``0xc0``.

Compatibility notes
-------------------

Fish 1.x and 2.x releases relied on the ``bc`` command for handling ``math`` expressions. Starting with fish 3.0.0 fish uses the tinyexpr library and evaluates the expression without the involvement of any external commands.

You don't need to use ``--`` before the expression even if it begins with a minus sign which might otherwise be interpreted as an invalid option. If you do insert ``--`` before the expression it will cause option scanning to stop just like for every other command and it won't be part of the expression.
