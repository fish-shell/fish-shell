function down-or-search -d "search forward or move down 1 line"
    # If we are already in search mode, continue
    if commandline --search-mode
        commandline -f history-search-forward
        return
    end

    # If we are navigating the pager, then up always navigates
    if commandline --paging-mode
        commandline -f down-line
        return
    end

    # We are not already in search mode.
    # If we are on the bottom line, start search mode,
    # otherwise move down
    set -l lineno (commandline -L)
    set -l line_count (count (commandline))

    switch $lineno
        case $line_count
            commandline -f history-search-forward

        case '*'
            commandline -f down-line
    end
end
