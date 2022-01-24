complete -c forfiles -f -a /P -d 'Specify the path from which to start the search'
complete -c forfiles -f -a /M -d 'Search files according to the specified search mask'
complete -c forfiles -f -a /S -d 'Instruct the forfiles command to search in subdirectories recursively'
complete -c forfiles -f -a /C -d 'Run the specified command on each file'
complete -c forfiles -f -a /D \
    -d 'Select files with a last modified date within the specified time frame'
complete -c forfiles -f -a '/?' -d 'Show help'
