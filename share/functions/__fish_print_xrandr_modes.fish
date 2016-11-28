function __fish_print_xrandr_modes --description 'Print xrandr modes'
    set -l out
    xrandr | sed '/^Screen/d; s/^ \+/mode: /' | while read -l output mode misc
        switch $output
            case mode:
                echo $mode\t(echo $misc | sed 's/\(^ \+\)\|\( *$\)//') [$out]
            case '*'
                set out $output
        end
    end


end
