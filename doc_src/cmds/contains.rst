.. _cmd-contains:

contains - test if a word is present in a list
==============================================

Synopsis
--------

::

    contains [OPTIONS] KEY [VALUES...]

Description
-----------

``contains`` tests whether the set ``VALUES`` contains the string ``KEY``. If so, ``contains`` exits with status 0; if not, it exits with status 1.

The following options are available:

- ``-i`` or ``--index`` print the word index

Note that, like GNU tools and most of fish's builtins, ``contains`` interprets all arguments starting with a ``-`` as options to contains, until it reaches an argument that is ``--`` (two dashes). See the examples below.

Example
-------

If $animals is a list of animals, the following will test if it contains a cat:



::

    if contains cat $animals
       echo Your animal list is evil!
    end


This code will add some directories to $PATH if they aren't yet included:



::

    for i in ~/bin /usr/local/bin
        if not contains $i $PATH
            set PATH $PATH $i
        end
    end


While this will check if ``hasargs`` was run with the ``-q`` option:



::

    function hasargs
        if contains -- -q $argv
            echo '$argv contains a -q option'
        end
    end


The ``--`` here stops ``contains`` from treating ``-q`` to an option to itself. Instead it treats it as a normal string to check.
