# Depending on cursor position and current mode, either search backward or move up one line"
function up-or-search -d "Search back or move cursor up 1 line"
    # If we are already in search mode, continue
    if commandline --search-mode
        commandline -f history-search-backward
        return
    end

    # If we are navigating the pager, then up always navigates
    if commandline --paging-mode
        commandline -f up-line
        return
    end

    # We are not already in search mode.
    # If we are on the top line, start search mode,
    # otherwise move up
    set -l lineno (commandline -L)

    switch $lineno
        case 1
            commandline -f history-search-backward

        case '*'
            commandline -f up-line
    end
end
