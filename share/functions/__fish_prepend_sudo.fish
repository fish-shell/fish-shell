function __fish_prepend_sudo -d "Prepend 'sudo ' to the beginning of the current commandline"
    set -l cmd (commandline -poc)
    if test "$cmd[1]" != "sudo"
        set -l cursor (commandline -C)
        commandline -C 0
        commandline -i "sudo "
        commandline -C (math $cursor + 5)
    end
end
