# Note that when a completion file is sourced a new block scope is created so `set -l` works.
set -l __fish_history_all_commands search delete save merge clear

# Note that these options are only valid with the "search" and "delete" subcommands.
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -s p -l prefix -d "Match items beginning with the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -s c -l contains -d "Match items containing the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -s e -l exact -d "Match items identical to the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -s t -l show-time -d "Output with timestamps"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -s C -l case-sensitive -d "Match items in a case-sensitive manner"
complete -c history -n '__fish_seen_subcommand_from search' \
    -s n -l max -d "Limit output to the first 'n' matches"

# We don't include a completion for the "save" subcommand because it should not be used
# interactively.
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a search -d "Prints commands from history matching the strings"
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a delete -d "Deletes commands from history matching the strings"
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a merge -d "Incorporate history changes from other sessions"
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a clear -d "Clears history file"
