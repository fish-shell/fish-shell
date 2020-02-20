.. _cmd-string:

string - manipulate strings
===========================

Synopsis
--------

::

    string collect [(-N | --no-trim-newlines)] [STRING...]
    string escape [(-n | --no-quoted)] [--style=xxx] [STRING...]
    string join [(-q | --quiet)] SEP [STRING...]
    string join0 [(-q | --quiet)] [STRING...]
    string length [(-q | --quiet)] [STRING...]
    string lower [(-q | --quiet)] [STRING...]
    string match [(-a | --all)] [(-e | --entire)] [(-i | --ignore-case)] [(-r | --regex)] [(-n | --index)] [(-q | --quiet)] [(-v | --invert)] PATTERN [STRING...]
    string repeat [(-n | --count) COUNT] [(-m | --max) MAX] [(-N | --no-newline)] [(-q | --quiet)] [STRING...]
    string replace [(-a | --all)] [(-f | --filter)] [(-i | --ignore-case)] [(-r | --regex)] [(-q | --quiet)] PATTERN REPLACEMENT [STRING...]
    string split [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] SEP [STRING...]
    string split0 [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] [STRING...]
    string sub [(-s | --start) START] [(-l | --length) LENGTH] [(-q | --quiet)] [STRING...]
    string trim [(-l | --left)] [(-r | --right)] [(-c | --chars CHARS)] [(-q | --quiet)] [STRING...]
    string unescape [--style=xxx] [STRING...]
    string upper [(-q | --quiet)] [STRING...]

Description
-----------

``string`` performs operations on strings.

STRING arguments are taken from the command line unless standard input is connected to a pipe or a file, in which case they are read from standard input, one STRING per line. It is an error to supply STRING arguments on the command line and on standard input.

Arguments beginning with ``-`` are normally interpreted as switches; ``--`` causes the following arguments not to be treated as switches even if they begin with ``-``. Switches and required arguments are recognized only on the command line.

Most subcommands accept a ``-q`` or ``--quiet`` switch, which suppresses the usual output but exits with the documented status.

The following subcommands are available.

"collect" subcommand
--------------------

.. include:: string-collect.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-collect.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-collect.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"escape" and "unescape" subcommands
-----------------------------------

.. include:: string-escape.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-escape.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-escape.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"join" and "join0" subcommands
------------------------------

.. include:: string-join.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-join.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-join.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"length" subcommand
-------------------

.. include:: string-length.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-length.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-length.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"lower" subcommand
------------------

.. include:: string-lower.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-lower.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

.. include:: string-lower.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"match" subcommand
------------------

.. include:: string-match.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-match.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

.. include:: string-match.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"repeat" subcommand
-------------------

.. include:: string-repeat.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-repeat.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-repeat.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"replace" subcommand
--------------------

.. include:: string-replace.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-replace.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

.. include:: string-replace.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

.. _cmd-string-split:
.. _cmd-string-split0:

"split" and "split0" subcommands
--------------------------------

.. include:: string-split.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-split.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-split.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"sub" subcommand
----------------

.. include:: string-sub.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-sub.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-sub.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"trim" subcommand
-----------------

.. include:: string-trim.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-trim.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

Examples
^^^^^^^^

.. include:: string-trim.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

"upper" subcommand
------------------

.. include:: string-upper.rst
   :start-after: BEGIN SYNOPSIS
   :end-before: END SYNOPSIS

.. include:: string-upper.rst
   :start-after: BEGIN DESCRIPTION
   :end-before: END DESCRIPTION

.. include:: string-upper.rst
   :start-after: BEGIN EXAMPLES
   :end-before: END EXAMPLES

Regular Expressions
-------------------

Both the ``match`` and ``replace`` subcommand support regular expressions when used with the ``-r`` or ``--regex`` option. The dialect is that of PCRE2.

In general, special characters are special by default, so ``a+`` matches one or more "a"s, while ``a\+`` matches an "a" and then a "+". ``(a+)`` matches one or more "a"s in a capturing group (``(?:XXXX)`` denotes a non-capturing group). For the replacement parameter of ``replace``, ``$n`` refers to the n-th group of the match. In the match parameter, ``\n`` (e.g. ``\1``) refers back to groups.

Some features include repetitions:

- ``*`` refers to 0 or more repetitions of the previous expression
- ``+`` 1 or more
- ``?`` 0 or 1.
- ``{n}`` to exactly n (where n is a number)
- ``{n,m}`` at least n, no more than m.
- ``{n,}`` n or more

Character classes, some of the more important:

- ``.`` any character except newline
- ``\d`` a decimal digit and ``\D``, not a decimal digit
- ``\s`` whitespace and ``\S``, not whitespace
- ``\w`` a "word" character and ``\W``, a "non-word" character
- ``[...]`` (where "..." is some characters) is a character set
- ``[^...]`` is the inverse of the given character set
- ``[x-y]`` is the range of characters from x-y
- ``[[:xxx:]]`` is a named character set
- ``[[:^xxx:]]`` is the inverse of a named character set
- ``[[:alnum:]]``  : "alphanumeric"
- ``[[:alpha:]]``  : "alphabetic"
- ``[[:ascii:]]``  : "0-127"
- ``[[:blank:]]``  : "space or tab"
- ``[[:cntrl:]]``  : "control character"
- ``[[:digit:]]``  : "decimal digit"
- ``[[:graph:]]``  : "printing, excluding space"
- ``[[:lower:]]``  : "lower case letter"
- ``[[:print:]]``  : "printing, including space"
- ``[[:punct:]]``  : "printing, excluding alphanumeric"
- ``[[:space:]]``  : "white space"
- ``[[:upper:]]``  : "upper case letter"
- ``[[:word:]]``   : "same as \w"
- ``[[:xdigit:]]`` : "hexadecimal digit"

Groups:

- ``(...)`` is a capturing group
- ``(?:...)`` is a non-capturing group
- ``\n`` is a backreference (where n is the number of the group, starting with 1)
- ``$n`` is a reference from the replacement expression to a group in the match expression.

And some other things:

- ``\b`` denotes a word boundary, ``\B`` is not a word boundary.
- ``^`` is the start of the string or line, ``$`` the end.
- ``|`` is "alternation", i.e. the "or".

Comparison to other tools
-------------------------

Most operations ``string`` supports can also be done by external tools. Some of these include ``grep``, ``sed`` and ``cut``.

If you are familiar with these, it is useful to know how ``string`` differs from them.

In contrast to these classics, ``string`` reads input either from stdin or as arguments. ``string`` also does not deal with files, so it requires redirections to be used with them.

In contrast to ``grep``, ``string``\ s `match` defaults to glob-mode, while `replace` defaults to literal matching. If set to regex-mode, they use PCRE regular expressions, which is comparable to ``grep``\ s `-P` option. `match` defaults to printing just the match, which is like ``grep`` with `-o` (use `--entire` to enable grep-like behavior).

Like ``sed``\ s `s/` command, ``string replace`` still prints strings that don't match. ``sed``\ s `-n` in combination with a `/p` modifier or command is like ``string replace -f``.

``string split somedelimiter`` is a replacement for ``tr somedelimiter \\n``.
