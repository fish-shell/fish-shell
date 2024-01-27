function __fish_eselect_cmd
    eselect --brief --colour=no $argv
end

function __fish_complete_eselect_modules
    set -l sedregexp 's/^  ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    __fish_eselect_cmd modules list | string match -r '^  ' | sed -r $sedregexp
end

function __fish_complete_eselect_actions
    set -l sedregexp 's/^  ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    set -l cmdl (commandline -pxc)
    __fish_eselect_cmd $cmdl[2..-1] usage | string match -r '^  [^ -]' | sed -r $sedregexp
end

function __fish_complete_eselect_action_options
    set -l parseregexp 's/^    ([a-zA-Z0-9_-]*)[ ]*/\1\t/g'
    set -l cmdl (commandline -pxc)

    # Alter further php completion
    if test (__fish_print_cmd_args_without_options)[2] = php
        eselect php list-modules 2>/dev/null | string split " "
        return
    end

    switch $cmdl[-1]
        case -'*'
            return
    end

    set -l findregexp '/^  '$cmdl[-1]'/,/^  [^ ]/p'

    set cmdl[-1] usage
    __fish_eselect_cmd $cmdl[2..-1] | sed -n -re $findregexp | string match -r '^    --' | sed -re $parseregexp
end

function __fish_complete_eselect_php_actions
    set -l sedregexp 's/^\s*\[([0-9]+)\]\s+([A-Za-z0-9\.]+).*/\1\t\2/'

    if test (__fish_print_cmd_args_without_options)[3] = set
        eselect php list (__fish_print_cmd_args_without_options)[-1] 2>/dev/null | sed -r $sedregexp
    end
end

function __fish_complete_eselect_targets
    set -l sedregexp 's/^  \[([0-9]+)\][ ]*/\1\t/g'
    set -l cmdl (commandline -pxc)

    # Disable further php completion
    if test (__fish_print_cmd_args_without_options)[2] = php
        return
    end

    switch $cmdl[-1]
        case -'*'
            set cmdl[-2] list
        case '*'
            set cmdl[-1] list
    end

    eselect --colour=no $cmdl[2..-1] | string match -r '^  [^ -]' | sed -r $sedregexp
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

complete -c eselect -n "test (__fish_number_of_cmd_args_wo_opts) = 4" \
    -xa '(__fish_complete_eselect_php_actions)'
