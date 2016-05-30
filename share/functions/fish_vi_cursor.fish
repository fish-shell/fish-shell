function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    set -l terminal $argv[1]
    set -q terminal[1]
    or set terminal auto
    set -l uses_echo

    set -l function
    switch "$terminal"
        case auto
            if set -q KONSOLE_PROFILE_NAME
               or set -q ITERM_PROFILE
                set function __fish_cursor_konsole
                set uses_echo 1
            else if string match -q "xterm*" -- $TERM
                set function __fish_cursor_xterm
                set uses_echo 1
            else
                return 1
            end
        case konsole
            set function __fish_cursor_konsole
            set uses_echo 1
        case xterm
            set function __fish_cursor_xterm
            set uses_echo 1
    end

    set -l tmux_prefix
    set -l tmux_postfix
    if set -q TMUX
       and set -q uses_echo[1]
        set tmux_prefix echo -ne "'\ePtmux;\e'"
        set tmux_postfix echo -ne "'\e\\\\'"
    end

    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block blink

    echo "
          function fish_vi_cursor_handle --on-variable fish_bind_mode
              set -l varname fish_cursor_\$fish_bind_mode
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $tmux_prefix
              $function \$\$varname
              $tmux_postfix
          end
         " | source
end

