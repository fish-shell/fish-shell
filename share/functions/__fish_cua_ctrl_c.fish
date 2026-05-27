# localization: skip(private)
function __fish_cua_ctrl_c
    # CUA Ctrl+C: copy if text is selected, otherwise cancel the current input.
    if test -n "$(commandline --current-selection)"
        commandline -f fish_clipboard_copy
    else
        commandline -f cancel
    end
end
