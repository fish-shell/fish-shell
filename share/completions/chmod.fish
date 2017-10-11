#
# Command specific completions for the chmod command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c chmod -s c -l changes -d 'Like verbose but report only when a change is made'
complete -c chmod -l no-preserve-root -d 'Do not treat / specially (the default)'
complete -c chmod -l preserve-root -d 'Fail to operate recursively on /'
complete -c chmod -s f -l silent -l quiet -d 'Suppress most error messages'
complete -c chmod -s v -l verbose -d 'Output a diagnostic for every file processed'
complete -c chmod -l reference -d 'Use RFILEs mode instead of MODE values' -r
complete -c chmod -s R -l recursive -d 'Change files and directories recursively'
complete -c chmod -l help -d 'Display help and exit'
complete -c chmod -l version -d 'Display version and exit'
