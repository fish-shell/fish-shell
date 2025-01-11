complete -c lazygit -xa "status branch log stash"
complete -c lazygit -s h -l help -d 'Display help'
complete -c lazygit -s v -l version -d 'Print version'
complete -c lazygit -s p -l path -d 'Path of git repo' -xa "(__fish_complete_directories)"

# Config
complete -c lazygit -s c -l config -d 'Print the default config'
complete -c lazygit -o cd -l print-config-dir -d 'Print the config directory'
complete -c lazygit -o ucd -l use-config-dir -d 'Override default config directory with provided directory' -r
complete -c lazygit  -o ucf -l use-config-file -d 'Comma separated list to custom config file(s)' -r

# Git
complete -c lazygit -s f -l filter -d 'Path to filter on in `git log -- <path>`' -r
complete -c lazygit -s g -l git-dir -d 'Equivalent of the --git-dir git argument' -r
complete -c lazygit -s w -l work-tree -d 'Equivalent of the --work-tree git argument' -xa "(__fish_complete_directories)"

# Debug
complete -c lazygit -s d -l debug -d 'Run in debug mode with logging'
complete -c lazygit -s l -l logs -d 'Tail lazygit logs; used with --debug'
complete -c lazygit -l profile -d 'Start the profiler and serve it on http port 6060'
