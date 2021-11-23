set --local CONDITION '! __fish_seen_argument --short r --long required-val --short o --long optional-val'

complete --command argparse --no-files

complete --command argparse --short-option h --long-option help --description 'Show help'

complete --command argparse --short-option n --long-option name --require-parameter --arguments '(functions --all | string replace ", " "\n")' --description 'Use function name'
complete --command argparse --short-option x --long-option exclusive --description 'Specify mutually exclusive options'
complete --command argparse --short-option N --long-option min-args --description 'Specify minimum non-option argument count'
complete --command argparse --short-option X --long-option max-args --description 'Specify maximum non-option argument count'
complete --command argparse --short-option i --long-option ignore-unknown --description 'Ignore unknown options'
complete --command argparse --short-option s --long-option stop-nonopt --description 'Exit on subcommand'
