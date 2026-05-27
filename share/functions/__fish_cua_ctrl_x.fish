# localization: skip(private)
function __fish_cua_ctrl_x
    # CUA Ctrl+X: cut if text is selected, otherwise kill to end of line.
    if test -n "$(commandline --current-selection)"
        commandline -f fish_clipboard_cut
    else
        commandline -f kill-line
    end
end
