# Completions for the 'asciinema' command

# general options
complete -c asciinema -s h -l help -d "Show this help message and exit"
complete -c asciinema -n __fish_no_arguments -l version -d "Show program's version number and exit"

# rec
complete -c asciinema -n __fish_use_subcommand -xa rec -d "Record terminal session"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l stdin -d "Enable stdin recording"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l append -d "Append to existing recording"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l raw -d "Save only raw stdout output"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l overwrite -d "Overwrite the file if it already exists"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s c -l command -d "Command to record" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s e -l env -d "List of environment variables to capture" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s t -l title -d "Title of the asciicast" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s i -l idle-time-limit -d "Limit recorded idle time to given number of seconds" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l cols -d "Override terminal columns for recorded process" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -l rows -d "Override terminal rows for recorded process" -x
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s y -l yes -d "Answer \"yes\" to all prompts"
complete -c asciinema -n "__fish_seen_subcommand_from rec" -s q -l quiet -d "Be quiet, suppress all notices/warnings"

# play
complete -c asciinema -n __fish_use_subcommand -xa play -d "Replay terminal session"
complete -c asciinema -n "__fish_seen_subcommand_from play" -s i -l idle-time-limit -d "Limit idle time during playback to given number of seconds" -x
complete -c asciinema -n "__fish_seen_subcommand_from play" -s s -l speed -d "Playback speedup" -x

# cat
complete -c asciinema -n __fish_use_subcommand -xa cat -d "Print full output of terminal session"

# upload
complete -c asciinema -n __fish_use_subcommand -xa upload -d "Upload locally saved terminal session to asciinema.org"

# auth
complete -c asciinema -n __fish_use_subcommand -xa auth -d "Manage recordings on asciinema.org account"
