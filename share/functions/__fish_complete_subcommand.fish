function __fish_complete_subcommand -d "Complete subcommand" --no-scope-shadowing
    # How many non-option tokens we skip in the input commandline before completing the subcommand
    # Usually 1; for ssh 2.
    set -l skip_next 1
    set -l subcommand
    while string match -rq -- '^--[a-z]' $argv[1]
        set -l arg $argv[1]
        set -e argv[1]
        switch $arg
            case '--fcs-skip=*'
                set skip_next (string split = -- $arg)[2]
            case --commandline # --commandline means to use our arguments instead of the commandline.
                complete -C "$argv"
                return
        end
    end
    set -l options_with_param $argv

    set -l cmd (commandline -cxp | string escape) (commandline -ct)
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

    printf "%s\n" (complete -C "$subcommand")
end
