function __fish_protontricks__complete_appid
    protontricks -l |
        string match --regex '.*\(\d+\)' |
        string replace --regex '(.*) \((\d+)\)' '$2\t$1'
end

complete -c protontricks-launch -f -s h -l help -d 'show this help message and exit'
complete -c protontricks-launch -l no-term -d 'Program was launched from desktop and no user-visible terminal is available. Error will be shown in a dialog instead of being printed.'
complete -c protontricks-launch -s v -l verbose -d 'Increase log verbosity. Can be supplied twice for maximum verbosity.'
complete -c protontricks-launch -l no-runtime -d 'Disable Steam Runtime'
complete -c protontricks-launch -l no-bwrap -d 'Disable bwrap containerization when using Steam Runtime'
complete -c protontricks-launch -l background-wineserver -d 'Launch a background wineserver process to improve Wine command startup time. Disabled by default, as it can cause problems with some graphical applications.'
complete -c protontricks-launch -l no-background-wineserver -d 'Do not launch a background wineserver process to improve Wine command startup time.'
complete -c protontricks-launch -l appid -xka '(__fish_protontricks__complete_appid)'
complete -c protontricks-launch -l cwd-app -d 'Set the working directory of launched executable to the Steam app\'s installation directory.'
