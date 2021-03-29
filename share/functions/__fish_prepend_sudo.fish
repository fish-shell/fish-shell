function __fish_prepend_sudo -d "Prepend 'sudo ' to the beginning of the current commandline"
    # If there is no commandline, insert the last item from history
    # and *then* toggle
    if not commandline | string length -q
        commandline -r "$history[1]"
    end

    set -l cmd (commandline -po)
    set -l cursor (commandline -C)

    if test "$cmd[1]" != sudo
        commandline -C 0
        commandline -i "sudo "
        commandline -C (math $cursor + 5)
    else
        commandline -r (string sub --start=6 (commandline -p))
        commandline -C -- (math $cursor - 5)
    end
end
