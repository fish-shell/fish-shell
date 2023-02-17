# Completions for the scrypt encryption utility

complete -x -c scrypt -n __fish_use_subcommand -a enc -d "Encrypt file"
complete -x -c scrypt -n __fish_use_subcommand -a dec -d "Decrypt file"
complete -x -c scrypt -n __fish_use_subcommand -a info -d "Print information about the encryption parameters"

complete -c scrypt -n "__fish_seen_subcommand_from enc dec" -s f -d "Force the operation to proceed"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -l logN -a "(seq 10 40)" -d "Set the work parameter N"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -s M -d "Use at most the specified bytes of RAM"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -s m -d "Use at most the specified fraction of the available RAM"
complete -c scrypt -n "__fish_seen_subcommand_from enc dec" -s P -d "Deprecated synonym for `--passphrase dev:stdin-once`"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -s p -a "(seq 1 32)" -d "Set the work parameter p"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -l passphrase -a "
    dev:tty-stdin\t'Read from /dev/tty, or stdin if fails (default)'
    dev:stdin-once\t'Read from stdin'
    dev:tty-once\t'Read from /dev/tty'
    env:(set -xn)\t'Read from the environment variable'
    file:(__fish_complete_path)\t'Read from the file'
    " -d "Read the passphrase using the specified method"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -s r -a "(seq 1 32)" -d "Set the work parameter r"
complete -x -c scrypt -n "__fish_seen_subcommand_from enc dec" -s t -d "Use at most the specified seconds of CPU time"
complete -c scrypt -n "__fish_seen_subcommand_from enc dec" -s v -d "Print encryption parameters and memory/CPU limits"
complete -x -c scrypt -n "not __fish_seen_subcommand_from enc dec info" -l version -d "Print version"
