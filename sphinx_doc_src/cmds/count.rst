count - count the number of elements of an array
==========================================

Synopsis
--------

count $VARIABLE


Description
------------

`count` prints the number of arguments that were passed to it. This is usually used to find out how many elements an environment variable array contains.

`count` does not accept any options, not even `-h` or `--help`.

`count` exits with a non-zero exit status if no arguments were passed to it, and with zero if at least one argument was passed.


Example
------------

\fish
count $PATH
# Returns the number of directories in the users PATH variable.

count *.txt
# Returns the number of files in the current working directory ending with the suffix '.txt'.
\endfish