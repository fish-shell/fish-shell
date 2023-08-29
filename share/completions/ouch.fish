# fish completions for ouch https://github.com/ouch-org/ouch

# subcommands list
set -l commands c compress d decompress l list help

# if no subcommand yet then no file completion
complete -c ouch -f --condition "not __fish_seen_subcommand_from $commands"

# subcommand completions
complete -c ouch --condition "not __fish_seen_subcommand_from $commands" \
    -a compress -d "Compress one or more files into one output file [aliases: c]"
complete -c ouch --condition "not __fish_seen_subcommand_from $commands" \
    -a decompress -d "Decompresses one or more files, optionally into another folder [aliases: d]"
complete -c ouch --condition "not __fish_seen_subcommand_from $commands" \
    -a list -d "List contents of an archive [aliases: l]"
complete -c ouch --condition "not __fish_seen_subcommand_from $commands" \
    -a help -d "Print this message or the help of the given subcommand(s)"

# options completions
complete -c ouch -s y -l yes -d "Skip [Y/n] questions positively"
complete -c ouch -s n -l no -d "Skip [Y/n] questions negatively"
complete -c ouch -s A -l accessible -d "Activate accessibility mode, reducing visual noise [env: ACCESSIBLE=]"
complete -c ouch -s H -l hidden -d "Ignores hidden files"
complete -c ouch -s g -l gitignore -d "Ignores files matched by git's ignore files"
complete -c ouch -s h -l help -d "Print help information (use `--help` for more detail)"
complete -c ouch -s V -l version -d "Print version information"
