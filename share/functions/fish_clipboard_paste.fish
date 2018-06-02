function fish_clipboard_paste
    set -l data
    if type -q pbpaste
        set data (pbpaste)
    else if type -q xsel
        # Return if `xsel` failed.
        # That way we don't print the redundant (and overly verbose for this) commandline error.
        # Also require non-empty contents to not clear the buffer.
        if not set data (xsel --clipboard 2>/dev/null)
            return 1
        end
    else if type -q xclip
        if not set data (xclip -selection clipboard -o 2>/dev/null)
            return 1
        end
    end
    # Also split on \r to turn it into a newline,
    # otherwise the output looks really confusing.
    set data (string split \r -- $data)

    # If the current token has an unmatched single-quote,
    # escape all single-quotes (and backslashes) in the paste,
    # in order to turn it into a single literal token.
    #
    # This eases pasting non-code (e.g. markdown or git commitishes).
    if __fish_commandline_is_singlequoted
        set data (string replace -ra "(['\\\])" '\\\\\\\$1' -- $data)
    end
    if test -n "$data"
        commandline -i -- $data
    end
end
