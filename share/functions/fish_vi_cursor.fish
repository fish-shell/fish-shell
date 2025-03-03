function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block

    function __fish_vi_cursor --argument-names varname
        if not status is-interactive; and not status is-interactive-read
            return
        end
        if not set -q $varname
            switch $varname
                case fish_cursor_insert
                    __fish_cursor_xterm line
                case fish_cursor_replace_one fish_cursor_replace
                    __fish_cursor_xterm underscore
                case '*'
                    __fish_cursor_xterm $fish_cursor_unknown
            end
            return
        end
        __fish_cursor_xterm $$varname
    end

    function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec --on-event fish_focus_in --on-event fish_read
        __fish_vi_cursor fish_cursor_$fish_bind_mode
    end

    function fish_vi_cursor_handle_preexec --on-event fish_preexec --on-event fish_exit
        set -l varname fish_cursor_external
        if not set -q $varname
            set varname fish_cursor_default
        end
        __fish_vi_cursor $varname
    end
end
