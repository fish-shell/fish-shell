function fish_default_mode_prompt --description "Display vi prompt mode"
    # Do nothing if not in vi mode
    if not contains -- "$fish_key_bindings" fish_vi_key_bindings fish_hybrid_key_bindings
        return
    end
    switch $fish_bind_mode
        case default
            set_color --bold red
            printf '[N]'
        case insert
            set_color --bold green
            printf '[I]'
        case replace_one
            set_color --bold green
            printf '[R]'
        case replace
            set_color --bold cyan
            printf '[R]'
        case visual
            set_color --bold magenta
            printf '[V]'
        case operator f F t T
            set_color --bold cyan
            printf '[N]'
    end
    set_color --reset
    printf ' '
end
