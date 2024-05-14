function __fish_paste
    # Also split on \r, otherwise it looks confusing
    set -l data (string split \r -- $argv[1] | string split \n)
    set data (string replace -ra [[:cntrl:]] '' -- $data)

    if commandline --search-field >/dev/null
        commandline --search-field -i -- $data
        return
    end

    # If the current token has an unmatched single-quote,
    # escape all single-quotes (and backslashes) in the paste,
    # in order to turn it into a single literal token.
    #
    # This eases pasting non-code (e.g. markdown or git commitishes).
    set -l quote_state (__fish_tokenizer_state -- (commandline -ct | string collect))
    if contains -- $quote_state single single-escaped
        if status test-feature regex-easyesc
            set data (string replace -ra "(['\\\])" '\\\\$1' -- $data)
        else
            set data (string replace -ra "(['\\\])" '\\\\\\\$1' -- $data)
        end
    else if not contains -- $quote_state double double-escaped
        and set -q data[2]
        # Leading whitespace in subsequent lines is unneded, since fish
        # already indents. Also gets rid of tabs (issue #5274).
        set -l tmp
        for line in $data
            switch $quote_state
                case normal
                    set -a tmp (string trim -l -- $line)
                case single single-escaped double double-escaped escaped
                    set -a tmp $line
            end
            set quote_state (__fish_tokenizer_state -i $quote_state -- $line)
        end
        set data $data[1] $tmp[2..]
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
