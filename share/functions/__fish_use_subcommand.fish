function __fish_use_subcommand -d "Test if a non-switch argument has been given in the current commandline"
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'
                continue
        end
        return 1
    end
    return 0
end
