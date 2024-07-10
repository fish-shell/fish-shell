string-escape - escape special characters
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string escape [-n | --no-quoted] [--style=] [STRING ...]
    string unescape [--style=] [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string escape`` escapes each *STRING* in one of three ways. The first is **--style=script**. This is the default. It alters the string such that it can be passed back to ``eval`` to produce the original argument again. By default, all special characters are escaped, and quotes are used to simplify the output when possible. If **-n** or **--no-quoted** is given, the simplifying quoted format is not used. Exit status: 0 if at least one string was escaped, or 1 otherwise.

**--style=var** ensures the string can be used as a variable name by hex encoding any non-alphanumeric characters. The string is first converted to UTF-8 before being encoded.

**--style=url** ensures the string can be used as a URL by hex encoding any character which is not legal in a URL. The string is first converted to UTF-8 before being encoded.

**--style=regex** escapes an input string for literal matching within a regex expression. The string is first converted to UTF-8 before being encoded.

``string unescape`` performs the inverse of the ``string escape`` command. For example, doing ``string unescape --style=var (string escape --style=var $str)`` will return the original string. It assumes its input is valid and typically coming unmodified from the output of the matching ``string escape`` operation. If the string to be unescaped is not properly formatted, it is ignored.

With regards to unescaping regex, in addition to reversing the effects of ``string escape --style=regex``, its counterpart ``string unescape --style=regex`` also unescapes *most* escape sequences that, in the most commonly used regex specs, *may* be escaped but are not in the current fish ``string escape`` implementation. For example, ``string escape --style=regex`` does not escape new lines or control characters and instead passes them through as-is (which is fine since they do not clash with any special regex syntax and *are* supported inputs to regex libraries), but to handle regex expressions formed from literal strings that were escaped outside of fish, ``string unescape --style=regex`` will also correctly detect and un-escape sequences like ``\n`` (literal backslash + n), mapping it to a new line. Note that ``string unescape --style=regex`` expects its input to be a string literal that has been previously escaped. Specifically, this means that it assumes all input is syntactically valid and well-formed regex and does not contain anything other than a literal regex pattern to match against (i.e. does not use special operators like ``|`` or ``(``/``)``, does not contain back references, does not make use of character classes, etc).

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ echo \x07 | string escape
    \cg

    >_ string escape --style=var 'a1 b2'\u6161
    a1_20_b2_E6_85_A1_


.. END EXAMPLES
