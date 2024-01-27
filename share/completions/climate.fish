# climate is a tool that provide simple commands that wrap some useful
# but complex combination of Linux commands.
# The tool can be found at https://github.com/adtac/climate

function __fish_climate_has_no_argument
    set -l cmd (commandline -xpc)
    not set -q cmd[2]
end

# Meta
complete -f -n __fish_climate_has_no_argument -c climate -a help -d 'Show help'
complete -f -n __fish_climate_has_no_argument -c climate -a update -d 'Update your climate install'
complete -f -n __fish_climate_has_no_argument -c climate -a uninstall -d 'uninstall climate'
complete -f -n __fish_climate_has_no_argument -c climate -a version -d 'Show climate version'

# Info
complete -f -n __fish_climate_has_no_argument -c climate -a weather -d 'Get the weather'

# General
complete -f -n __fish_climate_has_no_argument -c climate -a battery -d 'Display remaining battery'
complete -f -n __fish_climate_has_no_argument -c climate -a sleep -d 'Display remaining battery'
complete -f -n __fish_climate_has_no_argument -c climate -a lock -d 'Lock computer'
complete -f -n __fish_climate_has_no_argument -c climate -a shutdown -d 'Shutdown the computer'
complete -f -n __fish_climate_has_no_argument -c climate -a restart -d 'Restart the computer'
complete -f -n __fish_climate_has_no_argument -c climate -a time -d 'Show the time'
complete -f -n __fish_climate_has_no_argument -c climate -a clock -d 'Put a console clock in the top right corner'
complete -f -n __fish_climate_has_no_argument -c climate -a countdown -d 'A countdown timer'
complete -f -n __fish_climate_has_no_argument -c climate -a stopwatch -d 'A stopwatch'
complete -f -n __fish_climate_has_no_argument -c climate -a ix -d 'Pipe output to ix.io'

# Files
complete -n __fish_climate_has_no_argument -c climate -a biggest-files -d 'Find the biggest files recursively'
complete -n __fish_climate_has_no_argument -c climate -a biggest-dirs -d 'Find the biggest directories'
complete -n __fish_climate_has_no_argument -c climate -a dir-size -d 'Find directory size'
complete -n __fish_climate_has_no_argument -c climate -a remove-empty-dirs -d 'Remove empty directories'
complete -n __fish_climate_has_no_argument -c climate -a extract -d 'Extract any given archive'
complete -n __fish_climate_has_no_argument -c climate -a find-duplicates -d 'Report duplicate files in a directory'
complete -n __fish_climate_has_no_argument -c climate -a count -d 'Count the number of occurences'
complete -n __fish_climate_has_no_argument -c climate -a monitor -d 'Monitor file for changes'
complete -f -n __fish_climate_has_no_argument -c climate -a grep -d 'Search for the given pattern recursively'
complete -f -n __fish_climate_has_no_argument -c climate -a replace -d 'Replace all occurences'
complete -f -n __fish_climate_has_no_argument -c climate -a ramfs -d 'Create a ramfs of size (in MB) at path'

# Network
complete -f -n __fish_climate_has_no_argument -c climate -a speedtest -d 'Test your network speed'
complete -f -n __fish_climate_has_no_argument -c climate -a local-ip -d 'Retrieve your local ip address'
complete -f -n __fish_climate_has_no_argument -c climate -a is-online -d 'Verify if you\'re online'
complete -f -n __fish_climate_has_no_argument -c climate -a public-ip -d 'Retrieve your public ip address'
complete -f -n __fish_climate_has_no_argument -c climate -a ports -d 'List open ports'
complete -f -n __fish_climate_has_no_argument -c climate -a hosts -d 'Edit the hosts file'
complete -f -n __fish_climate_has_no_argument -c climate -a http-server -d 'http-server serving the current directory'
complete -f -n __fish_climate_has_no_argument -c climate -a is-up -d 'Determine if server is up'

# SSH
complete -f -n __fish_climate_has_no_argument -c climate -a download-file -d 'Download file from server'
complete -f -n __fish_climate_has_no_argument -c climate -a download-dir -d 'Download dir from server'
complete -n __fish_climate_has_no_argument -c climate -a upload -d 'Upload to server'
complete -f -n __fish_climate_has_no_argument -c climate -a ssh-mount -d 'Mount a remote path'
complete -n __fish_climate_has_no_argument -c climate -a ssh-unmount -d 'Unmount a ssh mount'

# git
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a undo-commit -d 'Undo the latest commit'
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a reset-locel -d 'Reset local repo to match remote'
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a pull-latest -d 'Seset local repo to match remote'
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a list-branches -d 'List all branches'
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a repo-size -d 'Calculate the repo size'
complete -f -c climate -n '__fish_is_git_repository; and __fish_climate_has_no_argument' -a user-stats -d 'Calculate total contribution for a user'

# Performance
complete -f -n __fish_climate_has_no_argument -c climate -a overview -d 'Display a performance overview'
complete -f -n __fish_climate_has_no_argument -c climate -a memory -d 'Find memory used'
complete -f -n __fish_climate_has_no_argument -c climate -a disk -d 'Find disk used'
complete -f -n __fish_climate_has_no_argument -c climate -a get-pids -d 'Get all PIDs for a process name'
complete -f -n __fish_climate_has_no_argument -c climate -a trash-size -d 'Find the trash size'
complete -f -n __fish_climate_has_no_argument -c climate -a empty -d 'Empty the trash'
