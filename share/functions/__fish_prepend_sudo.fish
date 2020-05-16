function __fish_prepend_sudo -d "Prepend 'sudo ' to the beginning of the current commandline"
    set -l cmd (commandline -po)
    set -l cursor (commandline -C)
    if test "$cmd[1]" != sudo
        commandline -C 0
        commandline -i "sudo "
        commandline -C (math $cursor + 5)
    else
        commandline -r (string sub --start=6 (commandline -p))
        set -l new_cursor (math $cursor - 5)
        if test "$new_cursor" -lt 0
          commandline -C 0
        else
          commandline -C $new_cursor
        end
    end
end
