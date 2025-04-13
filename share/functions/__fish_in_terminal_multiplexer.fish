function __fish_in_terminal_multiplexer
    set -l terminal_name "(status xtversion | string match -r '^\S*')"
    string match -q -- tmux $terminal_name ||
        __fish_in_gnu_screen ||
        # Emacs does probably not support multiplexing between multiple parent
        # terminals, but it is affected by the same issues.  Same for Vim's
        # :terminal TODO detect that before they implement XTGETTCAP.
        contains -- $TERM dvtm-256color eterm eterm-color
end
