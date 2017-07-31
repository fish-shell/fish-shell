# Sublime Text build 3142

# Usage: subl [arguments] [files]         edit the given files
#    or: subl [arguments] [directories]   open the given directories
#    or: subl [arguments] -               edit stdin

# Arguments:
#   --project <project>: Load the given project
#   --command <command>: Run the given command
#   -n or --new-window:  Open a new window
#   -a or --add:         Add folders to the current window
#   -w or --wait:        Wait for the files to be closed before returning
#   -b or --background:  Don't activate the application
#   -s or --stay:        Keep the application activated after closing the file
#   -h or --help:        Show help (this message) and exit
#   -v or --version:     Show version and exit

# --wait is implied if reading from stdin. Use --stay to not switch back
# to the terminal when a file is closed (only relevant if waiting for a file).

# Filenames may be given a :line or :line:column suffix to open at a specific
# location.

complete -c subl                  --long-option project    --require-parameter --description "Load the given project"
complete -c subl                  --long-option command    --require-parameter --description "Run the given command"
complete -c subl --short-option n --long-option new-window --no-files          --description "Open a new window"
complete -c subl --short-option a --long-option add        --require-parameter --description "Add folders to the current window"
complete -c subl --short-option w --long-option wait                           --description "Wait for the files to be closed before returning"
complete -c subl --short-option b --long-option background                     --description "Don't activate the application"
complete -c subl --short-option s --long-option stay                           --description "Keep the application activated after closing the file"
complete -c subl --short-option h --long-option help       --exclusive         --description "Show help and exit"
complete -c subl --short-option v --long-option version    --exclusive         --description "Show version and exit"
