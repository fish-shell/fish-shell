function __fish_tokenizer_state --description "Print the state of the tokenizer at the end of the given string"
    # Go through the token char-by-char in a state machine.
    # The states are:
    # - normal - no quoting is active (the starting state)
    # - single - open single-quote
    # - double - open double
    # - escaped - open \\ - the next character is non-special
    # - single-escaped - open \\ inside single-quotes
    # - double-escaped - open \\ inside double-quotes

    # Don't expect one arg only, as (commandline -ct) may evaluate to multiple arguments since
    # output is forcibly split on new lines (issue #7782).
    argparse --min-args 1 i/initial-state= -- $argv
    or return 1

    set -l state normal
    if set -q _flag_initial_state
        set str $_flag_initial_state
    end

    # HACK: We care about quotes and don't care about new lines, joining multi-line arguments
    # produced by (commandline -ct) on anything other than \n will corrupt the contents but will
    # allow the following logic to work (including escape of subsequent character). This entire
    # function should probably be implemented as part of the `commandline` builtin.
    for char in (string split -- "" (string join -- \x1e $argv))
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

    echo $state
end
