set __fish_eselect_cmd eselect --brief --colour=no

function __fish_complete_eselect_modules
    set -l sedregexp 's/^  ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    eval $__fish_eselect_cmd modules list | grep '^  ' | sed -r $sedregexp
end

function __fish_complete_eselect_actions
    set -l sedregexp 's/^  ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    set -l cmdl (commandline -poc)
    eval $__fish_eselect_cmd $cmdl[2..-1] help | grep '^  [^ -]' | sed -r $sedregexp
end

function __fish_complete_eselect_action_options
    set -l parseregexp 's/^    ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    set -l cmdl (commandline -poc)

    switch $cmdl[-1]
        case -'*'
            return
    end

    set -l findregexp '/^  '$cmdl[-1]'/,/^  [^ ]/p'

    set cmdl[-1] help
    eval $__fish_eselect_cmd $cmdl[2..-1] | sed -n -re $findregexp | grep '^    --' | sed -re $parseregexp
end

function __fish_complete_eselect_targets
    set -l sedregexp 's/^  \[([0-9]+)\][ ]*/\1\t/g'
    set -l cmdl (commandline -poc)

    switch $cmdl[-1]
        case -'*'
            set cmdl[-2] list
        case '*'
            set cmdl[-1] list
    end

    eval $cmdl[1] --colour=no $cmdl[2..-1] | grep '^  [^ -]' | sed -r $sedregexp
end

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -xa '(__fish_complete_eselect_modules)'

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -l brief -d 'Make output shorter'

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -l colour \
    -d "=<yes|no|auto> Enable or disable colour output (default 'auto')"

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 2" \
    -xa '(__fish_complete_eselect_actions)'

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 3" \
    -xa '(__fish_complete_eselect_targets)'

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 3" \
    -xa '(__fish_complete_eselect_action_options)'

