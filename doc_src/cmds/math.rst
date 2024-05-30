.. _cmd-math:

math - perform mathematics calculations
=======================================

Synopsis
--------

.. synopsis::

    math [(-s | --scale) N] [(-b | --base) BASE] [(-m | --scale-mode) MODE] EXPRESSION ...


Description
-----------

``math`` performs mathematical calculations.
It supports simple operations such as addition, subtraction, and so on, as well as functions like ``abs()``, ``sqrt()`` and ``ln()``.

By default, the output shows up to 6 decimal places.
To change the number of decimal places, use the ``--scale`` option, including ``--scale=0`` for integer output.

Keep in mind that parameter expansion happens before expressions are evaluated.
This can be very useful in order to perform calculations involving shell variables or the output of command substitutions, but it also means that parenthesis (``()``) and the asterisk (``*``) glob character have to be escaped or quoted.
``x`` can also be used to denote multiplication, but it needs to be followed by whitespace to distinguish it from hexadecimal numbers.

Parentheses for functions are optional - ``math sin pi`` prints ``0``.
However, a comma will bind to the inner function, so ``math pow sin 3, 5`` is an error because it tries to give ``sin`` the arguments ``3`` and ``5``.
When in doubt, use parentheses.

``math`` ignores whitespace between arguments and takes its input as multiple arguments (internally joined with a space), so ``math 2 +2`` and ``math "2 +    2"`` work the same.
``math 2 2`` is an error.

The following options are available:

**-s** *N* or **--scale** *N*
    Sets the scale of the result.
    ``N`` must be an integer or the word "max" for the maximum scale.
    A scale of zero causes results to be truncated by default. Any non-integer component is thrown away.
    So ``3/2`` returns ``1`` by default, rather than ``2`` which ``1.5`` would normally round to.
    This is for compatibility with ``bc`` which was the basis for this command prior to fish 3.0.0.
    Scale values greater than zero causes the result to be rounded using the usual rules to the specified number of decimal places.

**-b** *BASE* or **--base** *BASE*
    Sets the numeric base used for output (``math`` always understands hexadecimal numbers as input).
    It currently understands "hex" or "16" for hexadecimal and "octal" or "8" for octal and implies a scale of 0 (other scales cause an error), so it will truncate the result down to an integer.
    This might change in the future.
    Hex numbers will be printed with a ``0x`` prefix.
    Octal numbers will have a prefix of ``0`` but aren't understood by ``math`` as input.

**-m** *MODE* or **--scale-mode** *MODE*
    Sets scale behavior.
    The ``MODE`` can be ``truncate``, ``round``, ``floor``, ``ceiling``.
    The default value of scale mode is ``round`` with non zero scale and ``truncate`` with zero scale.

**-h** or **--help**
    Displays help about using this command.

Return Values
-------------

If the expression is successfully evaluated and doesn't over/underflow or return NaN the return ``status`` is zero (success) else one.

Syntax
------

``math`` knows some operators, constants, functions and can (obviously) read numbers.

For numbers, ``.`` is always the radix character regardless of locale - ``2.5``, not ``2,5``.
Scientific notation (``10e5``) and hexadecimal (``0xFF``) are also available.

``math`` allows you to use underscores as visual separators for digit grouping. For example, you can write ``1_000_000``, ``0x_89_AB_CD_EF``, and ``1.234_567_e89``.

Operators
---------

``math`` knows the following operators:

``+``
    for addition
``-``
    for subtraction
``*`` or ``x``
    for multiplication. ``*`` is the glob character and needs to be quoted or escaped, ``x`` needs to be followed by whitespace or it looks like ``0x`` hexadecimal notation.
``/``
    for division
``^``
    for exponentiation
``%``
    for modulo
``(`` or ``)``
    for grouping. These need to be quoted or escaped because ``()`` denotes a command substitution.

They are all used in an infix manner - ``5 + 2``, not ``+ 5 2``.

Constants
---------

``math`` knows the following constants:

``e``
    Euler's number
``pi``
    π, you know this one.
    Half of Tau
``tau``
    Equivalent to 2π, or the number of radians in a circle

Use them without a leading ``$`` - ``pi - 3`` should be about 0.

Functions
---------

``math`` supports the following functions:

``abs``
    the absolute value, with positive sign
``acos``
    arc cosine
``asin``
    arc sine
``atan``
    arc tangent
``atan2``
    arc tangent of two variables
``bitand``, ``bitor`` and ``bitxor``
    perform bitwise operations.
    These will throw away any non-integer parts and interpret the rest as an int.

    Note: ``bitnot`` and ``bitnand`` don't exist. This is because numbers in math don't really have a *width* in terms of bits,
    and these operations necessarily care about leading zeroes.

    If you need to negate a specific number you can do it with an xor with a mask, e.g.::

      > math --base=hex bitxor 0x0F, 0xFF
      0xF0

      > math --base=hex bitxor 0x2, 0x3
      # Here we mask with 0x3 == 0b111, so our number is 3 bits wide
      # Only the 1 bit isn't set.
      0x1

``ceil``
    round number up to the nearest integer
``cos``
    the cosine
``cosh``
    hyperbolic cosine
``exp``
    the base-e exponential function
``fac``
    factorial - also known as ``x!`` (``x * (x - 1) * (x - 2) * ... * 1``)
``floor``
    round number down to the nearest integer
``ln``
    the base-e logarithm
``log`` or ``log10``
    the base-10 logarithm
``log2``
    the base-2 logarithm
``max``
    returns the largest of the given numbers - this takes an arbitrary number of arguments (but at least one)
``min``
    returns the smallest of the given numbers - this takes an arbitrary number of arguments (but at least one)
``ncr``
    "from n choose r" combination function - how many subsets of size r can be taken from n (order doesn't matter)
``npr``
    the number of subsets of size r that can be taken from a set of n elements (including different order)
``pow(x,y)``
    returns x to the y (and can be written as ``x ^ y``)
``round``
    rounds to the nearest integer, away from 0
``sin``
    the sine function
``sinh``
    the hyperbolic sine
``sqrt``
    the square root - (can also be written as ``x ^ 0.5``)
``tan``
    the tangent
``tanh``
    the hyperbolic tangent

All of the trigonometric functions use radians (the pi-based scale, not 360°).

Examples
--------

``math 1+1`` outputs 2.

``math $status - 128`` outputs the numerical exit status of the last command minus 128.

``math 10 / 6`` outputs ``1.666667``.

``math -s0 10.0 / 6.0`` outputs ``1``.

``math -s3 10 / 6`` outputs ``1.667``.

``math "sin(pi)"`` outputs ``0``.

``math 5 \* 2`` or ``math "5 * 2"`` or ``math 5 "*" 2`` all output ``10``.

``math 0xFF`` outputs 255, ``math 0 x 3`` outputs 0 (because it computes 0 multiplied by 3).

``math bitand 0xFE, 0x2e`` outputs 46.

``math "bitor(9,2)"`` outputs 11.

``math --base=hex 192`` prints ``0xc0``.

``math 'ncr(49,6)'`` prints 13983816 - that's the number of possible picks in 6-from-49 lotto.

``math max 5,2,3,1`` prints 5.

Compatibility notes
-------------------

Fish 1.x and 2.x releases relied on the ``bc`` command for handling ``math`` expressions. Starting with fish 3.0.0 fish uses the tinyexpr library and evaluates the expression without the involvement of any external commands.

You don't need to use ``--`` before the expression, even if it begins with a minus sign which might otherwise be interpreted as an invalid option. If you do insert ``--`` before the expression, it will cause option scanning to stop just like for every other command and it won't be part of the expression.
