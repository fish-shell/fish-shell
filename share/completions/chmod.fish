#
# Command specific completions for the chmod command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c chmod -s c -l changes --description 'Like verbose but report only when a change is made'
complete -c chmod -l no-preserve-root --description 'Do not treat / specially (the default)'
complete -c chmod -l preserve-root --description 'Fail to operate recursively on /'
complete -c chmod -s f -l silent -l quiet --description 'Suppress most error messages'
complete -c chmod -s v -l verbose --description 'Output a diagnostic for every file processed'
complete -c chmod -l reference --description 'Use RFILEs mode instead of MODE values' -r
complete -c chmod -s R -l recursive --description 'Change files and directories recursively'
complete -c chmod -l help --description 'Display help and exit'
complete -c chmod -l version --description 'Display version and exit'
