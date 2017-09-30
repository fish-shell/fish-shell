function __fish_commandline_is_singlequoted --description "Return 0 if the current token has an open single-quote"
    # Go through the token char-by-char in a state machine.
    # The states are:
    # - normal - no quoting is active (the starting state)
    # - single - open single-quote
    # - double - open double
    # - escaped - open \\ - the next character is non-special
    # - single-escaped - open \\ inside single-quotes
    # - double-escaped - open \\ inside double-quotes

    set -l state normal
    for char in (commandline -ct | string split "")
        switch $char
            case "'" # single-quote
                switch $state
                    case normal single-escaped
                        set state single
                    case single
                        set state normal
                end
            case '"' # double-quote
                switch $state
                    case normal double-escaped
                        set state double
                    case double
                        set state normal
                end
            case \\ # backslash escapes the next character
                switch $state
                    case double
                        set state double-escaped
                    case double-escaped
                        set state double
                    case single
                        set state single-escaped
                    case single-escaped
                        set state single
                    case normal
                        set state escaped
                    case escaped
                        set state normal
                end
            case "*" # Any other character
                switch $state
                    case escaped
                        set state normal
                    case single-escaped
                        set state single
                    case double-escaped
                        set state double
                end
        end
    end
    # TODO: Should "single-escaped" also be a success?
    if contains -- $state single single-escaped
        return 0
    else
        return 1
    end
end
