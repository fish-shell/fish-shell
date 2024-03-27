set -l commands compile watch init query fonts update help

# global options
complete -c typst -n __fish_use_subcommand -f -l color -d 'Set when to use color' -a 'auto always never'
complete -c typst -n __fish_use_subcommand -r -l cert -d 'Path to custom CA certificate'
complete -c typst -n __fish_use_subcommand -f -l version -s v -d 'Print version'

# help option/subcommand
complete -c typst -f -l help -s h -d 'Print help'
complete -c typst -f -n __fish_use_subcommand -a help -d 'Print help for the given subcommand(s)'
complete -c typst -n '__fish_seen_subcommand_from help' -x -a "$commands"

# subcommands
complete -c typst -n __fish_use_subcommand -f -a compile -d 'Compile an input file'
complete -c typst -n __fish_use_subcommand -f -a watch -d 'Watch an input file and recompile on changes'
complete -c typst -n __fish_use_subcommand -f -a init -d 'Initialize a new project'
complete -c typst -n __fish_use_subcommand -f -a query -d 'Process an input file to extract metadata'
complete -c typst -n __fish_use_subcommand -f -a fonts -d 'List all discovered fonts'
complete -c typst -n __fish_use_subcommand -f -a update -d 'Self update the Typst CLI'

complete -c typst -n "__fish_seen_subcommand_from $commands" -x

# common subcommand options
# FIXME: only one input file
complete -c typst -n '__fish_seen_subcommand_from compile c watch w query' -x -ka '(__fish_complete_suffix .typ)' -d 'Input file'
#complete -c typst -n '__fish_seen_subcommand_from compile c watch w' -d 'Output file'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w query' -x -l root -a '(__fish_complete_directories)' -d 'Project root (for absolute paths)'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w query' -x -l input -d 'String key-value pair for `sys.inputs`'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w query fonts' -x -l font-path -a '(__fish_complete_directories)' -d 'Additional directories to search for fonts'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w query' -x -l diagnostic-format -a 'human short' -d 'Format to emit diagnostics in'

# compile/watch subcommands
complete -c typst -n '__fish_seen_subcommand_from compile c watch w' -x -l format -s f -a 'pdf png svg' -d 'Format of the output file'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w' -l open -d 'Open output file after compilation'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w' -x -l ppi -d 'Pixels per inch for PNG export'
complete -c typst -n '__fish_seen_subcommand_from compile c watch w' -l timings -d 'Produce performance timings'

# init subcommand
complete -c typst -n '__fish_seen_subcommand_from init' -n '__fish_is_nth_token 2' -x -d 'Template to use'
complete -c typst -n '__fish_seen_subcommand_from init' -n '__fish_is_nth_token 3' -x -a '(__fish_complete_directories)' -d 'Project directory'

# query subcommand
complete -c typst -n '__fish_seen_subcommand_from query' -x -l field -d 'Extract just one field'
complete -c typst -n '__fish_seen_subcommand_from query' -f -l one -d 'Retrieve exactly one element'
complete -c typst -n '__fish_seen_subcommand_from query' -x -l format -a 'json yaml' -d 'Format to serialize in'

# fonts subcommand
complete -c typst -n '__fish_seen_subcommand_from fonts' -f -l variants -d 'List style variants of each family'

# update subcommand
complete -c typst -n '__fish_seen_subcommand_from update' -f -l force -d 'Force a downgrade to an older version'
complete -c typst -n '__fish_seen_subcommand_from update' -f -l revert -d 'Revert to the version from before the last update'
