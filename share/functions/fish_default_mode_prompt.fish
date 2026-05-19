function fish_default_mode_prompt --description "Display vi prompt mode"
    # Do nothing if not in vi mode
    if not contains -- "$fish_key_bindings" fish_vi_key_bindings fish_hybrid_key_bindings
        return
    end
    switch $fish_bind_mode
        case default
            set -f color red
            set -f text NOR
        case insert
            set -f color green
            set -f text INS
        case replace_one
            set -f color green
            set -f text RE1
        case replace
            set -f color cyan
            set -f text REP
        case visual
            set -f color magenta
            set -f text VIS
        case operator f F t T
            set -f color cyan
            set -f text NOR
    end

    set_color --bold $color
    switch "$fish_mode_prompt_variant"
        case dim dimmed
            set_color --dim
            echo -n [

            set_color --reset --bold $color
            echo -n (string sub --length=1 -- $text)

            set_color --dim
            echo ]
        case triple 3 three
            echo $text
        case \*
            echo [(string sub --length=1 -- $text)]
    end
    set_color --reset
    echo -n ' '
end
