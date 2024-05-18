# Note that when a completion file is sourced a new block scope is created so `set -l` works.
set -l __fish_history_all_commands search delete save merge clear clear-session append

complete -c history -s h -l help -d "Display help and exit"

# Note that these options are only valid with the "search" and "delete" subcommands.
complete -c history -n '__fish_seen_subcommand_from search delete; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s p -l prefix -d "Match items beginning with the string"
complete -c history -n '__fish_seen_subcommand_from search delete; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s c -l contains -d "Match items containing the string"
complete -c history -n '__fish_seen_subcommand_from search delete; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s e -l exact -d "Match items identical to the string"
complete -c history -n '__fish_seen_subcommand_from search delete; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s t -l show-time -d "Output with timestamps"
complete -c history -n '__fish_seen_subcommand_from search delete; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s C -l case-sensitive -d "Match items in a case-sensitive manner"

# Note that these options are only valid with the "search" subcommand.
complete -c history -n '__fish_seen_subcommand_from search; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s n -l max -d "Limit output to the first 'n' matches" -x
complete -c history -n '__fish_seen_subcommand_from search; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s z -l null -d "Terminate entries with NUL character"
complete -c history -n '__fish_seen_subcommand_from search; or not __fish_seen_subcommand_from $__fish_history_all_commands' \
    -s R -l reverse -d "Output the oldest results first" -x

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
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a clear-session -d "Clears all history from the current session"
complete -f -c history -n "not __fish_seen_subcommand_from $__fish_history_all_commands" \
    -a append -d "Appends commands to the history without needing to execute them"
