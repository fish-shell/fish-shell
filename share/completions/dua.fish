# Options

set -l _sub_commands "aggregate a interactive i"
complete -c dua -n 'not __fish_seen_subcommand_from help' -s A -l apparent-size -d 'Display apparent size instead of disk usage'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s f -l format -f -r -d "The format with which to print byte counts" -a "
 metric\t'uses 1000 as base'
 binary\t'use 1024 as base'
 bytes\t'plain bytes without any formatting'
 GB\t'only gigabytes'
 GiB\t'only gibibytes'
 MB\t'only megabytes'
 MiB\t'only mebibytes'
"

complete -c dua -n 'not __fish_seen_subcommand_from help' -s h -l help -d 'Print help information'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s i -l ignore-dirs -r -F -d 'One or more absolute directories to ignore'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s l -l count-hard-links -d 'Count hard-linked files each time they are seen'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s t -l threads -r -f -d 'The amount of threads to use'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s V -l version -d 'Print version information'
complete -c dua -n 'not __fish_seen_subcommand_from help' -s x -l stay-on-filesystem -d 'If set, we will not cross filesystems or traverse mount points'

# Subcommands

complete -c dua -a aggregate -n "not __fish_seen_subcommand_from $_sub_commands help" -d 'Aggregrate the consumed space of one or more directories or files'
complete -c dua -a a -n "not __fish_seen_subcommand_from $_sub_commands help" -d 'Aggregrate the consumed space of one or more directories or files'

complete -c dua -n '__fish_seen_subcommand_from a aggregate' -l no-sort -d 'Do not sort paths by their size in bytes'
complete -c dua -n '__fish_seen_subcommand_from a aggregate' -l no-total -d 'Do not compute total column for multiple inputs'
complete -c dua -n '__fish_seen_subcommand_from a aggregate' -l stats -d 'Print additional statistics about the file traversal to stderr'

complete -c dua -a help -n "not __fish_seen_subcommand_from $_sub_commands" -f -d 'Print help message or the help of the given subcommand(s)'
complete -c dua -n "__fish_seen_subcommand_from help" -n "not __fish_any_arg_in $_sub_commands" -x -a "$_sub_commands"

complete -c dua -a interactive -n "not __fish_seen_subcommand_from $_sub_commands help" -d 'Launch the terminal user interface'
complete -c dua -a i -n "not __fish_seen_subcommand_from $_sub_commands help" -d 'Launch the terminal user interface'
