.. _cmd-string:

string - manipulate strings
===========================

Synopsis
--------

``string collect [(-n | --trim-newline)] [STRING...]``

``string escape [(-n | --no-quoted)] [--style=xxx] [STRING...]``

``string join [(-q | --quiet)] SEP [STRING...]``

``string join0 [(-q | --quiet)] [STRING...]``

``string length [(-q | --quiet)] [STRING...]``

``string lower [(-q | --quiet)] [STRING...]``

``string match [(-a | --all)] [(-e | --entire)] [(-i | --ignore-case)] [(-r | --regex)] [(-n | --index)] [(-q | --quiet)] [(-v | --invert)] PATTERN [STRING...]``

``string repeat [(-n | --count) COUNT] [(-m | --max) MAX] [(-N | --no-newline)] [(-q | --quiet)] [STRING...]``

``string replace [(-a | --all)] [(-f | --filter)] [(-i | --ignore-case)] [(-r | --regex)] [(-q | --quiet)] PATTERN REPLACEMENT [STRING...]``

``string split [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] SEP [STRING...]``

``string split0 [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] [STRING...]``

``string sub [(-s | --start) START] [(-l | --length) LENGTH] [(-q | --quiet)] [STRING...]``

``string trim [(-l | --left)] [(-r | --right)] [(-c | --chars CHARS)] [(-q | --quiet)] [STRING...]``

``string unescape [--style=xxx] [STRING...]``

``string upper [(-q | --quiet)] [STRING...]``


Description
-----------

``string`` performs operations on strings.

STRING arguments are taken from the command line unless standard input is connected to a pipe or a file, in which case they are read from standard input, one STRING per line. It is an error to supply STRING arguments on the command line and on standard input.

Arguments beginning with ``-`` are normally interpreted as switches; ``--`` causes the following arguments not to be treated as switches even if they begin with ``-``. Switches and required arguments are recognized only on the command line.

Most subcommands accept a ``-q`` or ``--quiet`` switch, which suppresses the usual output but exits with the documented status.

The following subcommands are available.

"collect" subcommand
--------------------

``string collect [(-n | --trim-newline)] [STRING...]``

``string collect`` collects its input into a single output argument, without splitting the output when used in a command substitution. This is useful when trying to collect multiline output from another command into a variable. Exit status: 0 if any output argument is non-empty, or 1 otherwise.

If invoked with multiple arguments instead of input, ``string collect`` preserves each argument separately, where the number of output arguments is equal to the number of arguments given to ``string collect``.

``--trim-newline`` trims a single trailing newline off of each output argument. This is useful when collecting the output from another command as the trailing newline is frequently not desired.

"escape" and "unescape" subcommands
-----------------------------------

``string escape [(-n | --no-quoted)] [--style=xxx] [STRING...]``

``string escape`` escapes each STRING in one of three ways. The first is ``--style=script``. This is the default. It alters the string such that it can be passed back to ``eval`` to produce the original argument again. By default, all special characters are escaped, and quotes are used to simplify the output when possible. If ``-n`` or ``--no-quoted`` is given, the simplifying quoted format is not used. Exit status: 0 if at least one string was escaped, or 1 otherwise.

``--style=var`` ensures the string can be used as a variable name by hex encoding any non-alphanumeric characters. The string is first converted to UTF-8 before being encoded.

``--style=url`` ensures the string can be used as a URL by hex encoding any character which is not legal in a URL. The string is first converted to UTF-8 before being encoded.

``--style=regex`` escapes an input string for literal matching within a regex expression. The string is first converted to UTF-8 before being encoded.

``string unescape [--style=xxx] [STRING...]``

``string unescape`` performs the inverse of the ``string escape`` command. If the string to be unescaped is not properly formatted it is ignored. For example, doing ``string unescape --style=var (string escape --style=var $str)`` will return the original string. There is no support for unescaping ``--style=regex``.

"join" subcommand
-----------------

``string join [(-q | --quiet)] SEP [STRING...]``

``string join`` joins its STRING arguments into a single string separated by SEP, which can be an empty string. Exit status: 0 if at least one join was performed, or 1 otherwise.

"join0" subcommand
------------------

``string join0 [(-q | --quiet)] [STRING...]``

``string join`` joins its STRING arguments into a single string separated by the zero byte (NUL), and adds a trailing NUL. This is most useful in conjunction with tools that accept NUL-delimited input, such as ``sort -z``. Exit status: 0 if at least one join was performed, or 1 otherwise.

"length" subcommand
-------------------

``string length [(-q | --quiet)] [STRING...]``

``string length`` reports the length of each string argument in characters. Exit status: 0 if at least one non-empty STRING was given, or 1 otherwise.

"lower" subcommand
------------------

``string lower [(-q | --quiet)] [STRING...]``

``string lower`` converts each string argument to lowercase. Exit status: 0 if at least one string was converted to lowercase, else 1. This means that in conjunction with the ``-q`` flag you can readily test whether a string is already lowercase.

"match" subcommand
------------------

``string match [(-a | --all)] [(-e | --entire)] [(-i | --ignore-case)] [(-r | --regex)] [(-n | --index)] [(-q | --quiet)] [(-v | --invert)] PATTERN [STRING...]``

``string match`` tests each STRING against PATTERN and prints matching substrings. Only the first match for each STRING is reported unless ``-a`` or ``--all`` is given, in which case all matches are reported.

If you specify the ``-e`` or ``--entire`` then each matching string is printed including any prefix or suffix not matched by the pattern (equivalent to ``grep`` without the ``-o`` flag). You can, obviously, achieve the same result by prepending and appending ``*`` or ``.*`` depending on whether or not you have specified the ``--regex`` flag. The ``--entire`` flag is simply a way to avoid having to complicate the pattern in that fashion and make the intent of the ``string match`` clearer. Without ``--entire`` and ``--regex``, a PATTERN will need to match the entire STRING before it will be reported.

Matching can be made case-insensitive with ``--ignore-case`` or ``-i``.

If ``--index`` or ``-n`` is given, each match is reported as a 1-based start position and a length. By default, PATTERN is interpreted as a glob pattern matched against each entire STRING argument. A glob pattern is only considered a valid match if it matches the entire STRING.

If ``--regex`` or ``-r`` is given, PATTERN is interpreted as a Perl-compatible regular expression, which does not have to match the entire STRING. For a regular expression containing capturing groups, multiple items will be reported for each match, one for the entire match and one for each capturing group. With this, only the matching part of the STRING will be reported, unless ``--entire`` is given.

If ``--invert`` or ``-v`` is used the selected lines will be only those which do not match the given glob pattern or regular expression.

Exit status: 0 if at least one match was found, or 1 otherwise.

"repeat" subcommand
-------------------

``string repeat [(-n | --count) COUNT] [(-m | --max) MAX] [(-N | --no-newline)] [(-q | --quiet)] [STRING...]``
  
``string repeat`` repeats the STRING ``-n`` or ``--count`` times. The ``-m`` or ``--max`` option will limit the number of outputted char (excluding the newline). This option can be used by itself or in conjunction with ``--count``. If both ``--count`` and ``--max`` are present, max char will be outputed unless the final repeated string size is less than max, in that case, the string will repeat until count has been reached. Both ``--count`` and ``--max`` will accept a number greater than or equal to zero, in the case of zero, nothing will be outputed. If ``-N`` or ``--no-newline`` is given, the output won't contain a newline character at the end. Exit status: 0 if yielded string is not empty, 1 otherwise.

"replace" subcommand
--------------------

``string replace [(-a | --all)] [(-f | --filter)] [(-i | --ignore-case)] [(-r | --regex)] [(-q | --quiet)] PATTERN REPLACEMENT [STRING...]``

``string replace`` is similar to ``string match`` but replaces non-overlapping matching substrings with a replacement string and prints the result. By default, PATTERN is treated as a literal substring to be matched.

If ``-r`` or ``--regex`` is given, PATTERN is interpreted as a Perl-compatible regular expression, and REPLACEMENT can contain C-style escape sequences like ``\t`` as well as references to capturing groups by number or name as ``$n`` or ``${n}``.

If you specify the ``-f`` or ``--filter`` flag then each input string is printed only if a replacement was done. This is useful where you would otherwise use this idiom: ``a_cmd | string match pattern | string replace pattern new_pattern``. You can instead just write ``a_cmd | string replace --filter pattern new_pattern``.

Exit status: 0 if at least one replacement was performed, or 1 otherwise.

.. _cmd-string-split:

"split" subcommand
------------------

``string split [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] SEP [STRING...]``

``string split`` splits each STRING on the separator SEP, which can be an empty string. If ``-m`` or ``--max`` is specified, at most MAX splits are done on each STRING. If ``-r`` or ``--right`` is given, splitting is performed right-to-left. This is useful in combination with ``-m`` or ``--max``. With ``-n`` or ``--no-empty``, empty results are excluded from consideration (e.g. ``hello\n\nworld`` would expand to two strings and not three). Exit status: 0 if at least one split was performed, or 1 otherwise.

See also ``read --delimiter``.

.. _cmd-string-split0:

"split0" subcommand
-------------------

``string split0 [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] [STRING...]``
  
``string split0`` splits each STRING on the zero byte (NUL). Options are the same as ``string split`` except that no separator is given.

``split0`` has the important property that its output is not further split when used in a command substitution, allowing for the command substitution to produce elements containing newlines. This is most useful when used with Unix tools that produce zero bytes, such as ``find -print0`` or ``sort -z``. See split0 examples below.

"sub" subcommand
----------------

``string sub [(-s | --start) START] [(-l | --length) LENGTH] [(-q | --quiet)] [STRING...]``
  
``string sub`` prints a substring of each string argument. The start of the substring can be specified with ``-s`` or ``--start`` followed by a 1-based index value. Positive index values are relative to the start of the string and negative index values are relative to the end of the string. The default start value is 1. The length of the substring can be specified with ``-l`` or ``--length``. If the length is not specified, the substring continues to the end of each STRING. Exit status: 0 if at least one substring operation was performed, 1 otherwise.

"trim" subcommand
-----------------

``string trim [(-l | --left)] [(-r | --right)] [(-c | --chars CHARS)] [(-q | --quiet)] [STRING...]``
  
``string trim`` removes leading and trailing whitespace from each STRING. If ``-l`` or ``--left`` is given, only leading whitespace is removed. If ``-r`` or ``--right`` is given, only trailing whitespace is trimmed. The ``-c`` or ``--chars`` switch causes the characters in CHARS to be removed instead of whitespace. Exit status: 0 if at least one character was trimmed, or 1 otherwise.

"upper" subcommand
------------------

``string upper [(-q | --quiet)] [STRING...]``

``string upper`` converts each string argument to uppercase. Exit status: 0 if at least one string was converted to uppercase, else 1. This means that in conjunction with the ``-q`` flag you can readily test whether a string is already uppercase.

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

In contrast to ``grep``, ``string``\ s `match` defaults to glob-mode, whie `replace` defaults to literal matching. If set to regex-mode, they use PCRE regular expressions, which is comparable to ``grep``\ s `-P` option. `match` defaults to printing just the match, which is like ``grep`` with `-o` (use `--entire` to enable grep-like behavior).

Like ``sed``\ s `s/` command, ``string replace`` still prints strings that don't match. ``sed``\ s `-n` in combination with a `/p` modifier or command is like ``string replace -f``.

``string split somedelimiter`` is a replacement for ``tr somedelimiter \\n``.

Examples
--------



::

    >_ string length 'hello, world'
    12
    
    >_ set str foo
    >_ string length -q $str; echo $status
    0
    # Equivalent to test -n $str




::

    >_ string sub --length 2 abcde
    ab
    
    >_ string sub -s 2 -l 2 abcde
    bc
    
    >_ string sub --start=-2 abcde
    de




::

    >_ string split . example.com
    example
    com
    
    >_ string split -r -m1 / /usr/local/bin/fish
    /usr/local/bin
    fish
    
    >_ string split '' abc
    a
    b
    c




::

    >_ seq 3 | string join ...
    1...2...3




::

    >_ string trim ' abc  '
    abc
    
    >_ string trim --right --chars=yz xyzzy zany
    x
    zan




::

    >_ echo \\x07 | string escape
    cg




::

    >_ string escape --style=var 'a1 b2'\\u6161
    a1_20b2__c_E6_85_A1




::

    >_ echo \"(echo one\ntwo\nthree | string collect)\"
    "one
    two
    three
    "
    
    >_ echo \"(ech one\ntwo\nthree | string collect -n)\"
    "one
    two
    three"


Match Glob Examples
-------------------



::

    >_ string match '?' a
    a
    
    >_ string match 'a*b' axxb
    axxb
    
    >_ string match -i 'a??B' Axxb
    Axxb
    
    >_ echo 'ok?' | string match '*\\?'
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
    foo
    foo2
    


Match Regex Examples
--------------------



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
    
    >_ string match -r '(\\d\\d?):(\\d\\d):(\\d\\d)' 2:34:56
    2:34:56
    2
    34
    56
    
    >_ string match -r '^(\\w{{2,4}})\\g1$' papa mud murmur
    papa
    pa
    murmur
    mur
    
    >_ string match -r -a -n at ratatat
    2 2
    4 2
    6 2
    
    >_ string match -r -i '0x[0-9a-f]{{1,8}}' 'int magic = 0xBadC0de;'
    0xBadC0de


NUL Delimited Examples
----------------------



::

    >_ # Count files in a directory, without being confused by newlines.
    >_ count (find . -print0 | string split0)
    42
    
    >_ # Sort a list of elements which may contain newlines
    >_ set foo beta alpha\\ngamma
    >_ set foo (string join0 $foo | sort -z | string split0)
    >_ string escape $foo[1]
    alpha\\ngamma


Replace Literal Examples
------------------------



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
----------------------



::

    >_ string replace -r -a '[^\\d.]+' ' ' '0 one two 3.14 four 5x'
    0 3.14 5
    
    >_ string replace -r '(\\w+)\\s+(\\w+)' '$2 $1 $$' 'left right'
    right left $
    
    >_ string replace -r '\\s*newline\\s*' '\\n' 'put a newline here'
    put a
    here


Repeat Examples
---------------



::

    >_ string repeat -n 2 'foo '
    foo foo
    
    >_ echo foo | string repeat -n 2
    foofoo
    
    >_ string repeat -n 2 -m 5 'foo'
    foofo
    
    >_ string repeat -m 5 'foo'
    foofo

