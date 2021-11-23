set --local CONDITION '! __fish_seen_argument -s r --long required-val -s o --long optional-val'

complete -c argparse -f

complete -c argparse -s h -l help -d 'Show help and exit'

complete -c argparse -s n -l name -d 'Specifies function name'
complete -c argparse -s x -l exclusive -d 'Specifies mutually exclusive options'
complete -c argparse -s N -l min-args -d 'Specifies minimum non-option arg count'
complete -c argparse -s X -l max-args -d 'Specifies maximum non-option arg count'
complete -c argparse -s i -l ignore-unknown -d 'Ignore unknown options'
complete -c argparse -s s -l stop-nonopt -d 'Exit on subcommand'
