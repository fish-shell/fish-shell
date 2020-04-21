function __fish_complete_proc
    # "comm=" means "print comm field with an empty name", which causes the header to be removed.
    # On many systems, comm is truncated (e.g. on Linux it's 15 chars),
    # but that's okay since commands that accept them as argument also only use those (e.g. pgrep).
    # String removes zombies (ones in parentheses) and padding (macOS adds some apparently).
    __fish_ps -o comm= | string match -rv '\(.*\)' | string trim
end
