set -g __fish_history_all_commands search delete save merge clear

# Note that these options are only valid with the "search" and "delete" subcommands.
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -l prefix -s p --description "Match items beginning with the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -l contains -s c --description "Match items containing the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -l exact -s e --description "Match items identical to the string"
complete -c history -n '__fish_seen_subcommand_from search delete' \
    -l with-time -s t --description "Output with timestamps"

# We don't include a completion for the "save" subcommand because it should not be used
# interactively.
complete -c history -f -n "not __fish_seen_subcommand_from $__fish_history_all_commands" -a search \
    --description "Prints commands from history matching the strings"
complete -c history -f -n "not __fish_seen_subcommand_from $__fish_history_all_commands" -a delete \
    --description "Deletes commands from history matching the strings"
complete -c history -f -n "not __fish_seen_subcommand_from $__fish_history_all_commands" -a merge \
    --description "Incorporate history changes from other sessions"
complete -c history -f -n "not __fish_seen_subcommand_from $__fish_history_all_commands" -a clear \
    --description "Clears history file"
