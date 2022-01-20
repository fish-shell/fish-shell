#
# Command specific completions for the bison command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c bison -s b -l file-prefix -d 'Specify a prefix to use for all output file names' -r
complete -c bison -s d -l defines -d 'Generate file with macro definitions for token type names'
complete -c bison -s g -l graph -d 'Output a VCG definition of the LALR(1) grammar automaton'
complete -c bison -s k -l token-table -d 'This switch causes the name'
complete -c bison -s l -l no-lines -d 'Dont put any #line preprocessor commands in the parser file'
complete -c bison -s n -l no-parser -d "Generate only declarations, not parser code"
complete -c bison -s o -l output -d 'Specify the name outfile for the parser file'
complete -c bison -s p -l name-prefix -d 'External symbols start with prefix instead of yy'
complete -c bison -s t -l debug -d 'Enable debugging facilities on compilation'
complete -c bison -s v -l verbose -d 'Generate file with descriptions of the parser states'
complete -c bison -s V -l version -d 'Print version number'
complete -c bison -s h -l help -d 'Print summary of the options'
complete -c bison -s y -l yacc -l fixed-output-files -d 'Equivalent to -o y.tab.c'
