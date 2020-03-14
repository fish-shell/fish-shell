function up-or-search -d "Depending on cursor position and current mode, either search backward or move up one line"
    # If we are already in search mode, continue
    if commandline --search-mode && set needle (commandline --search-term)
        set cursor (commandline --cursor)
        if test $cursor -ge $needle[1] && test $cursor -le $needle[2]
            commandline -f history-search-backward
            return
        end
    end

    # If we are navigating the pager, then up always navigates
    if commandline --paging-mode
        commandline -f up-line
        return
    end

    # We are not already in search mode.
    # If we are on the top line, start search mode,
    # otherwise move up
    set lineno (commandline -L)

    switch $lineno
        case 1
            commandline -f history-search-backward

        case '*'
            commandline -f up-line
    end
end
