function __fish_prepend_sudo -d "Prepend 'sudo ' to the beginning of the current commandline"
    set -l cmd (commandline -poc)
    if test "$cmd[1]" != "sudo"
        commandline -C 0
        commandline -i "sudo "
        commandline -f end-of-line
    end
end
