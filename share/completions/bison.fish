#
# Command specific completions for the bison command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c bison -s b -l file-prefix --description 'Specify a prefix to use for all bison output file names' -r

complete -c bison -s d --description 'Write an extra output file containing macro definitions for the token type names defined in the grammar and the semantic value type YYSTYPE, as well as a few extern variable declarations'
complete -c bison -l defines --description 'The behavior of --defines is the same than -d option'
complete -c bison -s g --description 'Output a VCG definition of the LALR(1) grammar automaton com puted by Bison'
complete -c bison -l graph --description 'The behavior of --graph is the same than -g option'
complete -c bison -s k -l token-table --description 'This switch causes the name'
complete -c bison -s l -l no-lines --description 'Dont put any #line preprocessor commands in the parser file'
complete -c bison -s n -l no-parser --description 'Do not generate the parser code into the output; generate only declarations'
complete -c bison -s o -l output --description 'Specify the name outfile for the parser file'
complete -c bison -s p -l name-prefix --description 'Rename the external symbols used in the parser so that they start with prefix instead of yy'
complete -c bison -s t -l debug --description 'In the parser file, define the macro YYDEBUG to 1 if it is not already defined, so that the debugging facilities are compiled'
complete -c bison -s v -l verbose --description 'Write an extra output file containing verbose descriptions of the parser states and what is done for each type of look-ahead token in that state'
complete -c bison -s V -l version --description 'Print the version number of bison and exit'
complete -c bison -s h -l help --description 'Print a summary of the options to bison and exit'
complete -c bison -s y -l yacc -l fixed-output-files --description 'Equivalent to -o y.tab.c'
