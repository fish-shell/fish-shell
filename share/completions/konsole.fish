# Completion for konsole

complete -c konsole -f

complete -c konsole -l help -xs h -d 'Show help and exit'
complete -c konsole -l help-all -d 'Show all options and exit'
complete -c konsole -l version -xs v -d 'Show version and exit'
complete -c konsole -l author -d 'Show author info and exit'
complete -c konsole -l license -d 'Show license info and exit'
complete -c konsole -l desktopfile -d 'Base file name of desktop entry'

set -l profiles (konsole --list-profiles)
complete -c konsole -l profile -d 'Name of profile to use'
for p in $profiles
    complete -c konsole -n "__fish_seen_subcommand_from --profile; and not __fish_seen_subcommand_from $profiles" -a $p
end

complete -c konsole -l layout -rF -d 'json layoutfile to be loaded'
complete -c konsole -l builtin-profile -d 'Use the built-in profile'
complete -c konsole -l workdir -d 'Set initial working dir' -xa "(__fish_complete_directories)"
complete -c konsole -l hold -l noclose -d 'Do not close when command exists'
complete -c konsole -l new-tab -d 'Create a new tab'
complete -c konsole -l tabs-from-file -rF -d 'Create tabs from tabs config'
complete -c konsole -l background-mode -d 'Start Konsole in the background'
complete -c konsole -l separate -l nofork -d 'Run in a separate process'
complete -c konsole -l show-menubar -d 'Show the menubar'
complete -c konsole -l hide-menubar -d 'Hide the menubar'
complete -c konsole -l show-tabbar -d 'Show the tabbar'
complete -c konsole -l hide-tabbar -d 'Hide the tabbar'
complete -c konsole -l fullscreen -d 'Start Konsole in fullscreen mode'
complete -c konsole -l notransparency -d 'Disable transparent backgrounds'
complete -c konsole -l list-profiles -x -d 'List the available profiles'
complete -c konsole -l list-profile-properties -x -d 'List all the profile properties'
complete -c konsole -s p -x -d '<P=V> Change profile property'
complete -c konsole -s e -x -d '<CMD> Command to execute'
complete -c konsole -l force-reuse -d 'Force re-using existing instance'
