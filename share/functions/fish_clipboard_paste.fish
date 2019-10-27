# Search for and set clipboard handler only once at startup
if type -q pbpaste
    function __fish_clipboard_paste
        pbpaste 2>/dev/null
    end
else if set -q WAYLAND_DISPLAY; and type -q wl-paste
    function __fish_clipboard_paste
        wl-paste 2>/dev/null
    end
else if type -q xsel
    function __fish_clipboard_paste
        xsel --clipboard 2>/dev/null
    end
else if type -q xclip
    function __fish_clipboard_paste
        xclip -selection clipboard -o 2>/dev/null
    end
end

function fish_clipboard_paste
    set -l data (__fish_clipboard_paste)

    # Issue 6254: Handle zero-length clipboard content
    if not string match -qr . -- "$data"
        return 1
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
        if status test-feature regex-easyesc
            set data (string replace -ra "(['\\\])" '\\\\$1' -- $data)
        else
            set data (string replace -ra "(['\\\])" '\\\\\\\$1' -- $data)
        end
    end
    if not string length -q -- (commandline -c)
        # If we're at the beginning of the first line, trim whitespace from the start,
        # so we don't trigger ignoring history.
        set data[1] (string trim -l -- $data[1])
    end
    if test -n "$data"
        commandline -i -- $data
    end
end
