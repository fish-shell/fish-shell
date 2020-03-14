function down-or-search -d "Depending on cursor position and current mode, either search forward or move down one line"
    # If we are already in search mode, continue
    if commandline --search-mode && set needle (commandline --search-term)
        set cursor (commandline --cursor)
        if test $cursor -ge $needle[1] && test $cursor -le $needle[2]
            commandline -f history-search-forward
            return
        end
    end

    # If we are navigating the pager, then up always navigates
    if commandline --paging-mode
        commandline -f down-line
        return
    end


    # We are not already in search mode.
    # If we are on the bottom line, start search mode,
    # otherwise move down
    set lineno (commandline -L)
    set line_count (count (commandline))

    switch $lineno
        case $line_count
            commandline -f history-search-forward

        case '*'
            commandline -f down-line
    end
end
