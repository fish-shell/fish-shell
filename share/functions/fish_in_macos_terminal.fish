function fish_in_macos_terminal
    test "$(status terminal-os || echo uname="$(__fish_uname)")" = uname=Darwin
end
