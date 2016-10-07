function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    # Since we read exported variables (KONSOLE_PROFILE_NAME and ITERM_PROFILE)
    # we need to check harder if we're actually in a supported terminal,
    # because we might be in a term-in-a-term (emacs ansi-term).
    if not contains -- $TERM xterm konsole xterm-256color konsole-256color
        and not set -q TMUX
        return
    end
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
            else if string match -q "xterm*" -- $TERM; or test "$VTE_VERSION" -gt 1910
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
          function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec
              set -l varname fish_cursor_\$fish_bind_mode
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $tmux_prefix
              $function \$\$varname
              $tmux_postfix
          end
         " | source

    echo "
          function fish_vi_cursor_handle_preexec --on-event fish_preexec
              set -l varname fish_cursor_default
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $tmux_prefix
              $function \$\$varname
              $tmux_postfix
          end
         " | source
end

