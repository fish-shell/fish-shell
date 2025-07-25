function __fish_protontricks_complete_winetricks_command
    complete -C 'winetricks '
end

function __fish_protontricks_is_search
    __fish_contains_opt -s s search
end

complete -c protontricks -f
complete -c protontricks -n 'not __fish_protontricks_is_search' -n '__fish_is_nth_token 1' -ka '(__fish_protontricks_complete_appid)'
complete -c protontricks -n 'not __fish_protontricks_is_search' -n 'not __fish_is_nth_token 1' -a '(__fish_protontricks_complete_winetricks_command)'
complete -c protontricks -s h -l help -d 'Show help message and exit'
complete -c protontricks -s v -l verbose -d 'Increase log verbosity, can be supplied twice'
complete -c protontricks -l no-term -d 'Specify that no terminal is available to Protontricks'
complete -c protontricks -s s -l search -d 'Search for game(s) with the given name'
complete -c protontricks -s l -l list -d 'List all apps'
complete -c protontricks -s c -l command -xa '(__fish_complete_subcommand)' -d 'Run a command with Wine-related environment variables set'
complete -c protontricks -l gui -d 'Launch the Protontricks GUI'
complete -c protontricks -l no-runtime -d 'Disable Steam Runtime'
complete -c protontricks -l no-bwrap -d 'Disable bwrap containerization when using Steam Runtime'
complete -c protontricks -l background-wineserver -d 'Launch a wineserver process to improve Wine startup time'
complete -c protontricks -l no-background-wineserver -d 'Do not launch wineserver process'
complete -c protontricks -l cwd-app -d 'Change to the Steam app directory when launching command'
complete -c protontricks -s V -l version -d 'Show version number and exit'
