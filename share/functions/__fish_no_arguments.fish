function __fish_no_arguments -d "Internal fish function"
    set -l cmd (commandline -pxc) (commandline -tc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'

            case '*'
                return 1
        end
    end
    return 0
end
