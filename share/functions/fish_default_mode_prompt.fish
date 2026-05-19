function fish_default_mode_prompt --description "Display vi prompt mode"
    # Do nothing if not in vi mode
    if not contains -- "$fish_key_bindings" fish_vi_key_bindings fish_hybrid_key_bindings
        return
    end
    switch $fish_bind_mode
        case default
            set -f color red
            set -f letter N
        case insert
            set -f color green
            set -f letter I
        case replace_one
            set -f color green
            set -f letter R
        case replace
            set -f color cyan
            set -f letter R
        case visual
            set -f color magenta
            set -f letter V
        case operator f F t T
            set -f color cyan
            set -f letter N
    end

    set_color --bold --dim $color
    echo -n \[

    set_color --reset --bold $color
    echo -n $letter

    set_color --dim
    echo \]

    set_color --reset
    echo -n ' '
end
