#Completions for make

function __fish_print_make_targets
	set files Makefile makefile GNUmakefile
	grep -h -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $files | cut -d ":" -f 1 | sed -r 's/^ *//;s/ *$//;s/ +/\n/g' ^/dev/null
end

complete -x -c make -a "(__fish_print_make_targets)" -d "Target"
complete -r -c make -s f -d "Use file as makefile" -r
complete -x -c make -s C -x -a "(__fish_complete_directory (commandline -ct))" -d "Change directory"
complete -c make -s d -d "Debug"
complete -c make -s e -d "Environment before makefile"
complete -c make -s i -d "Ignore errors"
complete -x -c make -s I -d "Search directory for Makefile" -a "(__fish_complete_directory (commandline -ct))"
complete -x -c make -s j -d "Number of jobs"
complete -c make -s k -d "Continue after an error"
complete -c make -s l -d "Start when load drops"
complete -c make -s n -d "Do not execute commands"
complete -c make -s o -r -d "Ignore specified file"
complete -c make -s p -d "Print database"
complete -c make -s q -d "Question mode"
complete -c make -s r -d "Eliminate implicit rules"
complete -c make -s s -d "Silent operation"
complete -c make -s S -d "Cancel the effect of -k"
complete -c make -s t -d "Touch files, dont run commands"
complete -c make -s v -d "Print version"
complete -c make -s w -d "Print working directory"
complete -c make -s W -r -d "Pretend file is modified"

