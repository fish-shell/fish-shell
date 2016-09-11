set -g __fish_history_all_commands search delete save merge clear

function __fish_history_cmd_in_array
        for i in (commandline -pco)
                # -- is used to provide no options for contains
                # (if $i is equal to --optname without -- will be error)
                if contains -- $i $argv
                        return 0
                end
        end

        return 1
end

function __fish_history_no_subcommand
        not __fish_history_cmd_in_array $__fish_history_all_commands
end

complete -c history -l prefix -s p --description "Match items beginning with the string"
complete -c history -l contains -s c --description "Match items containing the string"
complete -c history -l exact -s e --description "Match items identical to the string"
complete -c history -l with-time -s t --description "Output with timestamps"

# We don't include a completion for the "save" subcommand because it should not be used
# interactively.
complete -c history -f -n '__fish_history_no_subcommand' -a search \
    --description "Prints commands from history matching the strings"
complete -c history -f -n '__fish_history_no_subcommand' -a delete \
    --description "Deletes commands from history matching the strings"
complete -c history -f -n '__fish_history_no_subcommand' -a merge \
    --description "Incorporate history changes from other sessions"
complete -c history -f -n '__fish_history_no_subcommand' -a clear \
    --description "Clears history file"
