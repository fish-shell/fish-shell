#!/bin/sh

# This little script calls the man command to render a manual page and
# pipes the result into the gen_hdr2 program to convert the output
# into a C string literal. 

# NAME is the name of the function we are generating documentation for.
NAME=`basename $1 .doxygen`

# Render the page
nroff -man doc_src/builtin_doc/man/man1/${NAME}.1 | col -b | cat -s | sed -e '$d' | ./gen_hdr2

