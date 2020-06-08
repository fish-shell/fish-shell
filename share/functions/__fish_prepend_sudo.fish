function __fish_prepend_sudo -d "Prepend 'sudo ' to the beginning of the current commandline"
    set -l cmd (commandline -po)
    set -l cursor (commandline -C)
    if test -z "$cmd"
        commandline -r "sudo $history[1]"
    else if test "$cmd[1]" != sudo
        commandline -C 0
        commandline -i "sudo "
        commandline -C (math $cursor + 5)
    else
        commandline -r (string sub --start=6 (commandline -p))
        commandline -C -- (math $cursor - 5)
    end
end
