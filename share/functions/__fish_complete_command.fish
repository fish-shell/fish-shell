function __fish_complete_command --description 'Complete using all available commands'
    set -l ctoken "$(commandline -ct)"
    switch $ctoken
        case '*=*'
            set ctoken (string split "=" -- $ctoken)
            complete -C "$ctoken[2]"
        case '-*' # do not try to complete options as commands
            return
        case '*'
            complete -C "$ctoken"
    end
end
