.. _cmd-count:

count - count the number of elements of a list
================================================

Synopsis
--------

::

    count $VARIABLE
    COMMAND | count
    count < FILE

Description
-----------

``count`` prints the number of arguments that were passed to it, plus the number of newlines passed to it via stdin. This is usually used to find out how many elements an environment variable list contains, or how many lines there are in a text file.

``count`` does not accept any options, not even ``-h`` or ``--help``.

``count`` exits with a non-zero exit status if no arguments were passed to it, and with zero if at least one argument was passed.

Note that, like ``wc -l``, reading from stdin counts newlines, so ``echo -n foo | count`` will print 0.

Example
-------



::

    count $PATH
    # Returns the number of directories in the users PATH variable.
    
    count *.txt
    # Returns the number of files in the current working directory ending with the suffix '.txt'.

    git ls-files --others --exclude-standard | count
    # Returns the number of untracked files in a git repository

    printf '%s\n' foo bar | count baz
    # Returns 3 (2 lines from stdin plus 1 argument)

    count < /etc/hosts
    # Counts the number of entries in the hosts file
