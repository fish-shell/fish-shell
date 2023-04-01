function ghoti_default_mode_prompt --description "Display vi prompt mode"
    # Do nothing if not in vi mode
    if test "$ghoti_key_bindings" = ghoti_vi_key_bindings
        or test "$ghoti_key_bindings" = ghoti_hybrid_key_bindings
        switch $ghoti_bind_mode
            case default
                set_color --bold red
                echo '[N]'
            case insert
                set_color --bold green
                echo '[I]'
            case replace_one
                set_color --bold green
                echo '[R]'
            case replace
                set_color --bold cyan
                echo '[R]'
            case visual
                set_color --bold magenta
                echo '[V]'
        end
        set_color normal
        echo -n ' '
    end
end
