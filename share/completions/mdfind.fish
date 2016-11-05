# completion for mdfind (macOS)

complete -c mdfind -o attr -x -d 'Fetches the value of the specified attribute'
complete -c mdfind -o count -f -d 'Query only reports matching items count'
complete -c mdfind -o onlyin -x -a '(__fish_complete_directories (commandline -ct))' -d 'Search only within given directory'
complete -c mdfind -o live -f -d 'Query should stay active'
complete -c mdfind -o name -x -d 'Search on file name only'
complete -c mdfind -o reprint -f -d 'Reprint results on live update'
complete -c mdfind -s s -x -d 'Show contents of smart folder'
complete -c mdfind -s 0 -f -d 'Use NUL (\0) as a path separator, for use with xargs -0'
complete -c mdfind -o literal -f -d 'Force the provided query string to be taken as a literal'
complete -c mdfind -o interpret -f -d 'Interprete query string as Spotlight query'
