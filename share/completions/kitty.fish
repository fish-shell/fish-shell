function __kitty_list_listen_on_arguments
    echo -e "tcp:\tTCP port"
    for variable in (set --names --export)
        echo -e "unix:@$variable\tEnvironment variable"
    end
end

complete -c kitty -s h -l help -d "Show [h]elp"
complete -c kitty -s v -l version -d "Show [v]ersion"

complete -c kitty -l class -f -d "Set the class part of the WM_CLASS property"
complete -c kitty -l name -f -d "Set the name part of the WM_CLASS property"
complete -c kitty -s T -l title -f -d "Set the window [T]itle"
complete -c kitty -s c -l config -f -d "Set the path to the [c]onfiguration file(s) to use"
complete -c kitty -s o -l override -f -d "[o]verride individual configuration options, can be specified multiple times"

set directory_description  "Set the working [d]irectory"
complete -c kitty -s d -l directory -f -d $directory_description
complete -c kitty -l working-directory -f -d $directory_description

complete -c kitty -l detach -d "Detach from the controlling terminal"
complete -c kitty -l session -d "Set path to the file containing the startup session"
complete -c kitty -s w -l watcher -d "Set path to the python file"
complete -c kitty -l hold -d "Remain open after the child process exits"

complete -c kitty -s 1 -l single-instance -f -d "Run only the [s]ingle instance"
complete -c kitty -l instance-group -d "Used in combination with the --single-instance option"
complete -c kitty -l wait-for-single-instance-window-close -d "Don't quit till the newly opened window is closed"
complete -c kitty -l listen-on -a "(__kitty_list_listen_on_arguments)" -d "Listen to the address for control messages"
complete -c kitty -l start-as -a "fullscreen normal minimized maximized" -f -d "Set the initial window state"
complete -c kitty -l dump-commands -d "Show commands received from the child process to stdout"
complete -c kitty -l replay-commands -d "Replay previously dumped commands"
complete -c kitty -l dump-bytes -d "Set path to the file in which to store the raw bytes received from the child process"

set debug_rendering_description  "Debug rendering commands"
complete -c kitty -l debug-gl -f -d $debug_rendering_description
complete -c kitty -l debug-rendering -f -d $debug_rendering_description

set debug_input_description "Show out key and mouse events as they are received"
complete -c kitty -l debug-input -f -d $debug_input_description
complete -c kitty -l debug-keyboard -f -d $debug_input_description

complete -c kitty -l debug-font-fallback -d "Show the information about selection of fallback fonts"
