function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    # if stdin is not a tty, there is effectively no bind mode.
    if not test -t 0
        return
    end

    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block

    echo "
          function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec --on-event fish_focus_in --on-event fish_read
              set -l varname fish_cursor_\$fish_bind_mode
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              __fish_cursor_xterm \$\$varname
          end
         " | source

    echo "
          function fish_vi_cursor_handle_preexec --on-event fish_preexec
              set -l varname fish_cursor_external
              if not set -q \$varname
                set varname fish_cursor_default
              end
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              __fish_cursor_xterm \$\$varname
          end
         " | source
end
