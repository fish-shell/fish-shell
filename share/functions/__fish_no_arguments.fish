
function __ghoti_no_arguments -d "Internal ghoti function"
    set -l cmd (commandline -poc) (commandline -tc)
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
