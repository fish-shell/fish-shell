function __fish_wmctrl__complete_workspace_ids
    wmctrl -d | awk '{ printf $1 "\t" $NF "\n" }'
end

function __fish_wmctrl__complete_mvargs
    set -l current_token (commandline -tc)
    set comma_count (string match --all --regex ',' -- "$current_token" |
        count)

    test $comma_count -lt 4 && echo "$current_token,"
end

function __fish_wmctrl__complete_geometry
    set -l current_token (commandline -tc)

    switch "$current_token"
        case ','
            return
        case '*'
            printf '%s,' "$current_token"
    end
end

function __fish_wmctrl__complete_window_titles
    wmctrl -l | string replace --regex '^[^ ]+\s+[^ ]+\s+[A-Za-z\-/]+\s+' ''

    printf '%s\n' :ACTIVE:\t"The active window" \
        :SELECT:\t"Select window"
end

function __fish_wmctrl__complete_window_properties
    set -l current_token (commandline -tc | string replace --regex '[^,]*$' '')

    switch "$current_token"
        case '*,*'
            set -l properties modal \
                sticky \
                maximized_vert \
                maximized_horz \
                shaded \
                skip_taskbar \
                skip_pager \
                hidden \
                fullscreen \
                above \
                below

            set -l suffix ,

            set -l comma_count (string match --all --regex ',' -- "$current_token" |
                count)

            test $comma_count -ge 2 && set suffix
            test $comma_count -ge 3 && set properties

            for property in $properties
                printf '%s%s%s\n' "$current_token" "$property" "$suffix"
            end

        case '*'
            printf '%s,\n' add\t"Add the property" \
                remove\t"Remove the property" \
                toggle\t"Toggle the property"
    end
end

set -l command wmctrl
complete -c $command -f

complete -c $command -s h -l help -d 'Show help'

complete -c $command -s a -x \
    -a '(__fish_wmctrl__complete_window_titles)' \
    -d 'Go to the desktop with a specific window'

complete -c $command -s b -x \
    -a '(__fish_wmctrl__complete_window_properties)' \
    -d 'Edit the properties of a window'

complete -c $command -s c -x \
    -a '(__fish_wmctrl__complete_window_titles)' \
    -d 'Close the window'

complete -c $command -s d -d 'List the desktops'

complete -c $command -s e -x \
    -a '(__fish_wmctrl__complete_mvargs)' \
    -d 'Resize and move the window'

complete -c $command -s g -x \
    -a '(__fish_wmctrl__complete_geometry)' \
    -d 'Change the geometry of desktops'

complete -c $command -s I -x -d 'Change the icon of a window'

complete -c $command -s k -x \
    -a 'on off' \
    -d 'Whether to turn on window manager "show the desktop" mode'

complete -c $command -s l -d 'List the windows'
complete -c $command -s m -d 'Show information about the window manager'
complete -c $command -s n -x -d 'Change the amount of desktops'
complete -c $command -s N -x -d 'Change the name of a window'

complete -c $command -s o -x \
    -a '(__fish_wmctrl__complete_geometry)' \
    -d 'Change the viewport for a current desktop'

complete -c $command -s r -x \
    -a '(__fish_wmctrl__complete_window_titles)' \
    -d 'Specify the window for an action'

complete -c $command -s R -x \
    -a '(__fish_wmctrl__complete_window_titles)' \
    -d 'Move the window to a current desktop'

complete -c $command -s s -x \
    -a '(__fish_wmctrl__complete_workspace_ids)' \
    -d 'Switch to the desktop'

complete -c $command -s t -x \
    -a '(__fish_wmctrl__complete_workspace_ids)' \
    -d 'Move the window to the desktop'

complete -c $command -s T -x \
    -d 'Change the name and the icon of a window'

complete -c $command -s F \
    -d 'Treat window names as case sensitive ones and exact matches'

complete -c $command -s G \
    -d 'Include the geometry information in a window list'

complete -c $command -s i -d 'Expect the window IDs instead of their names'
complete -c $command -s p -d 'Include the PIDs in a window list'
complete -c $command -s u -d 'Enable UTF-8 mode'
complete -c $command -s v -d 'Show the verbose output'

complete -c $command -s w -x \
    -a DESKTOP_TITLES_INVALID_UTF8 \
    -d 'Enable the workarounds'

complete -c $command -s x \
    -d 'Include WM_CLASS in a window list'
