set command json_schemer

complete -c $command -s h -l help -d 'Show [h]elp'
complete -c $command -s v -l version -d 'Show [v]ersion'

complete -c $command \
    -s e \
    -l errors \
    -d 'Specify the maximum number of [e]rrors for validated files' \
    -x
