function __fish_complete_subcommand -d "Complete subcommand" --no-scope-shadowing
    # Pass --commandline to complete the remainder of the arguments instead of the commandline.
    # Pass --allow-functions-and-builtins to enable the completion of the first token as function or builtin.
    # Other args are considered flags to the supercommand that require an option.

    # How many non-option tokens we skip in the input commandline before completing the subcommand
    # Usually 1; for ssh 2.
    set -l skip_next 1
    set -l allow_functions_and_builtins false
    set -l subcommand
    while string match -rq -- '^--[a-z]' $argv[1]
        set -l arg $argv[1]
        set -e argv[1]
        switch $arg
            case '--fcs-skip=*'
                set skip_next (string split = -- $arg)[2]
            case '--allow-functions-and-builtins'
                set allow_functions_and_builtins true
            case '--commandline'
                set subcommand $argv
                set -e argv
                break
        end
    end
    set -l options_with_param $argv

    if not string length -q -- $subcommand
        set cmd (commandline -cop) (commandline -ct)
        while set -q cmd[1]
            set -l token $cmd[1]
            set -e cmd[1]
            if contains -- $token $options_with_param
                set skip_next (math $skip_next + 1)
                continue
            end
            switch $token
                case '-*' '*=*'
                    continue
                case '*'
                    if test $skip_next -gt 0
                        set skip_next (math $skip_next - 1)
                        continue
                    end
                    # found the start of our command
                    set subcommand $token $cmd
                    break
            end
        end
    end

    if test $allow_functions_and_builtins = false && test (count $subcommand) -eq 1
        __fish_complete_external_command
    else
        printf "%s\n" (complete -C "$subcommand")
    end

end
