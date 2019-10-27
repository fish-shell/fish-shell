string-escape - escape special characters
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string escape [(-n | --no-quoted)] [--style=xxx] [STRING...]
    string unescape [--style=xxx] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string escape`` escapes each STRING in one of three ways. The first is ``--style=script``. This is the default. It alters the string such that it can be passed back to ``eval`` to produce the original argument again. By default, all special characters are escaped, and quotes are used to simplify the output when possible. If ``-n`` or ``--no-quoted`` is given, the simplifying quoted format is not used. Exit status: 0 if at least one string was escaped, or 1 otherwise.

``--style=var`` ensures the string can be used as a variable name by hex encoding any non-alphanumeric characters. The string is first converted to UTF-8 before being encoded.

``--style=url`` ensures the string can be used as a URL by hex encoding any character which is not legal in a URL. The string is first converted to UTF-8 before being encoded.

``--style=regex`` escapes an input string for literal matching within a regex expression. The string is first converted to UTF-8 before being encoded.

``string unescape`` performs the inverse of the ``string escape`` command. If the string to be unescaped is not properly formatted it is ignored. For example, doing ``string unescape --style=var (string escape --style=var $str)`` will return the original string. There is no support for unescaping ``--style=regex``.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ echo \\x07 | string escape
    cg

    >_ string escape --style=var 'a1 b2'\\u6161
    a1_20b2__c_E6_85_A1


.. END EXAMPLES
