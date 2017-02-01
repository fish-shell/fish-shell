function fish_clipboard_paste
    if type -q pbpaste
        commandline -i -- (pbpaste)
    else if type -q xsel
        # Only run `commandline` if `xsel` succeeded.
        # That way any xsel error is printed (to show e.g. a non-functioning X connection),
        # but we don't print the redundant (and overly verbose for this) commandline error.
        # Also require non-empty contents to not clear the buffer.
        if set -l data (xsel --clipboard)
            and test -n "$data"
            commandline -i -- $data
        end
    end
end
