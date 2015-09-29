# Completions for make

# This completion reenables file completion on
# assignments, so e.g. 'make foo FILES=<tab>' will receive standard
# filename completion.
complete -c make -n 'commandline -ct | string match "*=*"'

complete -x -c make -a "(__fish_print_make_targets)" --description "Target"
complete -r -c make -s f --description "Use file as makefile" -r
complete -x -c make -s C -x -a "(__fish_complete_directories (commandline -ct))" --description "Change directory"
complete -c make -s d --description "Debug mode"
complete -c make -s e --description "Environment before makefile"
complete -c make -s i --description "Ignore errors"
complete -x -c make -s I --description "Search directory for makefile" -a "(__fish_complete_directories (commandline -ct))"
complete -x -c make -s j --description "Number of concurrent jobs"
complete -c make -s k --description "Continue after an error"
complete -c make -s l --description "Start when load drops"
complete -c make -s n --description "Do not execute commands"
complete -c make -s o -r --description "Ignore specified file"
complete -c make -s p --description "Print database"
complete -c make -s q --description "Question mode"
complete -c make -s r --description "Eliminate implicit rules"
complete -c make -s s --description "Quiet mode"
complete -c make -s S --description "Don't continue after an error"
complete -c make -s t --description "Touch files, don't run commands"
complete -c make -s v --description "Display version and exit"
complete -c make -s w --description "Print working directory"
complete -c make -s W -r --description "Pretend file is modified"

