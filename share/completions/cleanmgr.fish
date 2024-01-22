function __cleanmgr_complete_args -d 'Function to generate args'
    set -l previous_token (commandline -xc)[-1]

    if test "$previous_token" = /d
        __fish_print_windows_drives
        return
    end
end

complete -c cleanmgr -f -a '(__cleanmgr_complete_args)'

complete -c cleanmgr -f -a /d -d 'Specify the drive'
complete -c cleanmgr -f -a /sageset \
    -d 'Display the Disk Cleanup Settings dialog box and also create a registry key'
complete -c cleanmgr -f -a /sagerun -d 'Run the specified tasks that are assigned to the n value'
complete -c cleanmgr -f -a /tuneup -d 'Run /sageset and /sagerun for the same n'
complete -c cleanmgr -f -a /lowdisk -d 'Run with the default settings'
complete -c cleanmgr -f -a /verylowdisk -d 'Run with the default settings, no user prompts'
complete -c cleanmgr -f -a '/?' -d 'Show help'
