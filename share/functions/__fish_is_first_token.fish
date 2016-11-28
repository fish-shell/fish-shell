
function __fish_is_first_token -d 'Test if no non-switch argument has been specified yet'
    set cmd (commandline -poc)
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

