complete -c archlinux-java -f
complete -c archlinux-java -n __fish_use_subcommand -a status -d 'List installed Java environments and enabled one'
complete -c archlinux-java -n __fish_use_subcommand -a set -d 'Force <JAVA_ENV> as default'
complete -c archlinux-java -n __fish_use_subcommand -a unset -d 'Unset current default Java environment'
complete -c archlinux-java -n __fish_use_subcommand -a get -d 'Return the short name of the Java environment set as default'
complete -c archlinux-java -n __fish_use_subcommand -a fix -d 'Fix an invalid/broken default Java environment configuration'
complete -c archlinux-java -n __fish_use_subcommand -a help -d 'Show help'

complete -c archlinux-java -n "__fish_seen_subcommand_from set" -a "(archlinux-java status | tail -n +2 | tr -s ' ' | cut -d ' ' -f2)"
