function fish_in_macos_terminal
    test "$(status terminal-os || echo "$(__fish_uname)")" = Darwin
end
