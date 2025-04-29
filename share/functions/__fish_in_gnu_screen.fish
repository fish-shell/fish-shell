function __fish_in_gnu_screen
    test -n "$STY" || contains -- $TERM screen screen-256color
end
