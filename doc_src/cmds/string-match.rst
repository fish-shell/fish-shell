string-match - match substrings
===============================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string match [(-a | --all)] [(-e | --entire)] [(-i | --ignore-case)] [(-r | --regex)] [(-n | --index)] [(-q | --quiet)] [(-v | --invert)] PATTERN [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string match`` tests each STRING against PATTERN and prints matching substrings. Only the first match for each STRING is reported unless ``-a`` or ``--all`` is given, in which case all matches are reported.

If you specify the ``-e`` or ``--entire`` then each matching string is printed including any prefix or suffix not matched by the pattern (equivalent to ``grep`` without the ``-o`` flag). You can, obviously, achieve the same result by prepending and appending ``*`` or ``.*`` depending on whether or not you have specified the ``--regex`` flag. The ``--entire`` flag is simply a way to avoid having to complicate the pattern in that fashion and make the intent of the ``string match`` clearer. Without ``--entire`` and ``--regex``, a PATTERN will need to match the entire STRING before it will be reported.

Matching can be made case-insensitive with ``--ignore-case`` or ``-i``.

If ``--index`` or ``-n`` is given, each match is reported as a 1-based start position and a length. By default, PATTERN is interpreted as a glob pattern matched against each entire STRING argument. A glob pattern is only considered a valid match if it matches the entire STRING.

If ``--regex`` or ``-r`` is given, PATTERN is interpreted as a Perl-compatible regular expression, which does not have to match the entire STRING. For a regular expression containing capturing groups, multiple items will be reported for each match, one for the entire match and one for each capturing group. With this, only the matching part of the STRING will be reported, unless ``--entire`` is given.

When matching via regular expressions, it is possible to directly import matches as fish variables by means of named capture groups using the PCRE2 syntax. This behavior is automatic and occurs in addition to any other match reporting. The default behavior with ``--regex`` results in the initialization of fish variables in the default scope containing the matched text corresponding to each named capture group. A named capture group matching a zero-length string will be initialized as a fish variable containing a likewise empty string (i.e. the equivalent of ``""``), but a named capture group that did not match will result in an empty (null) fish variable.  When ``--regex`` is used with ``--all``, this behavior changes. Each named variable will contain a list of matches, with the first match contained in the first element, the second match in the second, and so on. If the group was empty or did not match, the corresponding element will be an empty string.

If ``--invert`` or ``-v`` is used the selected lines will be only those which do not match the given glob pattern or regular expression.

Exit status: 0 if at least one match was found, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

Match Glob Examples
^^^^^^^^^^^^^^^^^^^

::

    >_ string match '?' a
    a

    >_ string match 'a*b' axxb
    axxb

    >_ string match -i 'a??B' Axxb
    Axxb

    >_ echo 'ok?' | string match '*\?'
    ok?

    # Note that only the second STRING will match here.
    >_ string match 'foo' 'foo1' 'foo' 'foo2'
    foo

    >_ string match -e 'foo' 'foo1' 'foo' 'foo2'
    foo1
    foo
    foo2

    >_ string match 'foo?' 'foo1' 'foo' 'foo2'
    foo1
    foo2

Match Regex Examples
^^^^^^^^^^^^^^^^^^^^

::

    >_ string match -r 'cat|dog|fish' 'nice dog'
    dog

    >_ string match -r -v "c.*[12]" {cat,dog}(seq 1 4)
    dog1
    dog2
    cat3
    dog3
    cat4
    dog4

    >_ string match -r '(\d\d?):(\d\d):(\d\d)' 2:34:56
    2:34:56
    2
    34
    56

    >_ string match -r '^(\w{2,4})\1$' papa mud murmur
    papa
    pa
    murmur
    mur

    >_ string match -r -a -n at ratatat
    2 2
    4 2
    6 2

    >_ string match -r -i '0x[0-9a-f]{1,8}' 'int magic = 0xBadC0de;'
    0xBadC0de

    >_ echo $version
    3.1.2-1575-ga2ff32d90
    >_ string match -rq '(?<major>\d+).(?<minor>\d+).(?<revision>\d+)' -- $version
    >_ echo "You are using fish $major!"
    You are using fish 3!

    >_ string match -raq ' *(?<sentence>[^.!?]+)(?<punctuation>[.!?])?' "hello, friend. goodbye"
    >_ printf "%s\n" -- $sentence
    hello, friend
    goodbye
    >_ printf "%s\n" -- $punctuation
    .

    >_ string match -rq '(?<word>hello)' 'hi'
    >_ count $word
    0

.. END EXAMPLES
