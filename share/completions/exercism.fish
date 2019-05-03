function __fish_exercism_no_subcommand -d 'Test if exercism has yet to be given the subcommand'
    for i in (commandline -opc)
        if contains -- $i demo debug configure fetch restore submit unsubmit tracks download help
            return 1
        end
    end
    return 0
end

complete -c exercism -s c -l config -d 'path to config file [$EXERCISM_CONFIG_FILE, $XDG_CONFIG_HOME]'
complete -c exercism -s v -l verbose -d "turn on verbose logging"
complete -c exercism -s h -l help -d "show help"
complete -c exercism -s v -l version -d "print the version"
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'configure' -d "Writes config values to a JSON file."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'debug' -d "Outputs useful debug information."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'download' -d "Downloads a solution given the ID of the latest iteration."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'fetch' -d "Fetches the next unsubmitted problem in each track."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'list' -d "Lists the available problems for a language track, given its ID."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'open' -d "Opens exercism.io to your most recent iteration of a problem given the track ID and problem slug."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'restore' -d "Downloads the most recent iteration for each of your solutions on exercism.io."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'skip' -d "Skips a problem given a track ID and problem slug."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'status' -d "Fetches information about your progress with a given language track."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'submit' -d "Submits a new iteration to a problem on exercism.io."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'tracks' -d "Lists the available language tracks."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'unsubmit' -d "REMOVED"
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'upgrade' -d "Upgrades the CLI to the latest released version."
complete -f -n '__fish_exercism_no_subcommand' -c exercism -a 'help' -d "show help"
