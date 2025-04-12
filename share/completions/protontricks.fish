function __fish_protontricks__complete_appid
    protontricks -l |
        string match --regex '.*\(\d+\)' |
        string replace --regex '(.*) \((\d+)\)' '$2\t$1'
end

function __fish_protontricks__complete_winetricks_command
    complete -C 'winetricks '
end

function __fish_protontricks__is_search
    __fish_contains_opt -s s search
end

complete -c protontricks -f
complete -c protontricks -n 'not __fish_protontricks__is_search' -n '__fish_is_nth_token 1' -ka '(__fish_protontricks__complete_appid)'
complete -c protontricks -n 'not __fish_protontricks__is_search' -n 'not __fish_is_nth_token 1' -a '(__fish_protontricks__complete_winetricks_command)'
complete -c protontricks -s h -l help -d 'show this help message and exit'
complete -c protontricks -s v -l verbose -d 'Increase log verbosity. Can be supplied twice for maximum verbosity.'
complete -c protontricks -l no-term -d 'Program was launched from desktop. This is used automatically when lauching Protontricks from desktop and no user-visible terminal is available.'
complete -c protontricks -s s -l search -d 'Search for game(s) with the given name'
complete -c protontricks -s l -l list -d 'List all apps'
complete -c protontricks -s c -l command -xa '(__fish_complete_subcommand)' -d 'Run a command with Wine-related environment variables set. The command is passed to the shell as-is without being escaped.'
complete -c protontricks -l gui -d 'Launch the Protontricks GUI.'
complete -c protontricks -l no-runtime -d 'Disable Steam Runtime'
complete -c protontricks -l no-bwrap -d 'Disable bwrap containerization when using Steam Runtime'
complete -c protontricks -l background-wineserver -d 'Launch a background wineserver process to improve Wine command startup time. Disabled by default, as it can cause problems with some graphical applications.'
complete -c protontricks -l no-background-wineserver -d 'Do not launch a background wineserver process to improve Wine command startup time.'
complete -c protontricks -l cwd-app -d 'Set the working directory of launched command to the Steam app\'s installation directory.'
complete -c protontricks -s V -l version -d 'show program\'s version number and exit'
