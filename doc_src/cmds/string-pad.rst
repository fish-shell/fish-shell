string-pad - pad characters before and after string
===================================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string pad [(-l | --left)] [(-r | --right)] [(-c | --char) CHAR] [(-n | --count) INTEGER] [(-m | --max) INTEGER] [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string pad`` pads before and after the string specified character for each STRING.

The default behaviour is left padding with spaces and default count or max is zero.

If ``-l`` or ``--left`` is given, only padded before string.

If ``-r`` or ``--right`` is given, only padded after string.

The ``-c`` or ``--char`` switch causes the characters in CHAR to be padded instead of default whitespace  character.

The ``-n`` or ``--count`` integer specifies the amount of characters to be padded.

The ``-m`` or ``--max`` integer specifies the resulting length of the after after adding pad characters.

Note, that the ``-n`` and ``-m`` flags are mutually exclusive, you can only use one of them.

Exit status: 0 if string was padded, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string pad -l -n 10 -c ' ' 'abc'
              abc

    >_ string pad --right --count 5 --char=z foo bar
    foozzzzz
    barzzzzz

    >_ string pad --left --right -n 5 --char=- foo
    -----foo-----


.. END EXAMPLES
