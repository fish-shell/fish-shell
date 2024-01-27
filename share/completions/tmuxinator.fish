function __fish_tmuxinator_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

complete -f -c tmuxinator -a '(tmuxinator completions start)'
complete -f -c tmuxinator -a '(tmuxinator commands)'
complete -f -c tmuxinator -n '__fish_tmuxinator_using_command start' -a '(tmuxinator completions start)'
complete -f -c tmuxinator -n '__fish_tmuxinator_using_command open' -a '(tmuxinator completions open)'
complete -f -c tmuxinator -n '__fish_tmuxinator_using_command copy' -a '(tmuxinator completions copy)'
complete -f -c tmuxinator -n '__fish_tmuxinator_using_command delete' -a '(tmuxinator completions delete)'
