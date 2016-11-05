# completion for mdutil (macOS)

function __fish_mdutil_volumes
    command mdutil -a -s | while read -l line
        if string match -q \t"*" -- $line
            printf "%s\n" $line
        else
            # Use printf to not output a newline so indented lines are joined
            # to non-indented ones
            printf "%s" (string replace -r ':$' '' -- $line)
        end
    end
end

complete -c mdutil -s p -f -d 'Publish metadata'
complete -c mdutil -s i -f -a 'on off' -d 'Turn indexing on or off'
complete -c mdutil -s d -f -d 'Disable Spotlight activity for volume'
complete -c mdutil -s E -f -d 'Erase and rebuild index'
complete -c mdutil -s s -f -d 'Print indexing status'
complete -c mdutil -s t -x -a '(__fish_mdutil_volumes)' -d 'Resolve files from file id with an optional volume path or device id'
complete -c mdutil -s a -f -d 'Apply command to all volumes'
complete -c mdutil -s V -x -a '(__fish_mdutil_volumes)' -d 'Apply command to all stores on the specified volume'
complete -c mdutil -s v -f -d 'Display verbose information'
complete -c mdutil -x -a '(__fish_mdutil_volumes)'
