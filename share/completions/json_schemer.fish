set -l command json_schemer

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s v -l version -d 'Show version'

complete -c $command -s e -l errors -x \
    -d 'Specify the maximum number of errors for validated files'
