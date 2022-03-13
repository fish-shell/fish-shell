# completion for mdfind (macOS)

complete mdfind -d "find files matching given query"
complete mdfind -o attr -x -d 'Show given attribute'
complete mdfind -o count -f -d 'Print number of matches'
complete mdfind -o onlyin -x -a '(__fish_complete_directories (commandline -ct))' -d 'Search within directory'
complete mdfind -o live -f -d 'Query should stay active'
complete mdfind -o name -x -d 'Search on file name only'
complete mdfind -o reprint -f -d 'Reprint -live results'
complete mdfind -s s -x -d 'List a smart folder'
complete mdfind -s 0 -f -d 'NUL path separators'
complete mdfind -o literal -f -d 'Literal metadata query'
complete mdfind -o interpret -f -d 'Spotlight-like search'
