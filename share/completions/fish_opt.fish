set -g CONDITION '! __fish_seen_argument -s r --long required-val -s o --long optional-val'

complete -c fish_opt -f

complete -c fish_opt -s h -l help -d 'Show help and exit'

complete -c fish_opt -s s -l short -d 'Specifies short option'
complete -c fish_opt -s l -l long -d 'Specifies long option'
complete -c fish_opt -l longonly -d 'Use only long option'
complete -c fish_opt -s o -l optional-val -n $CONDITION -d 'Don\'t require value'
complete -c fish_opt -s r -l required-val -n $CONDITION  -d 'Require value'
complete -c fish_opt -l multiple-vals -d 'Store all values'
