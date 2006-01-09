#Completions for make

function __fish_print_make_targets
	set files Makefile makefile GNUmakefile
	grep -h -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $files | cut -d ":" -f 1 | sed -e 's/^ *//;s/ *$//;s/  */\n/g' ^/dev/null
end

complete -x -c make -a "(__fish_print_make_targets)" -d (_ "Target")
complete -r -c make -s f -d (_ "Use file as makefile") -r
complete -x -c make -s C -x -a "(__fish_complete_directory (commandline -ct))" -d (_ "Change directory")
complete -c make -s d -d (_ "Debug mode")
complete -c make -s e -d (_ "Environment before makefile")
complete -c make -s i -d (_ "Ignore errors")
complete -x -c make -s I -d (_ "Search directory for makefile") -a "(__fish_complete_directory (commandline -ct))"
complete -x -c make -s j -d (_ "Number of concurrent jobs")
complete -c make -s k -d (_ "Continue after an error")
complete -c make -s l -d (_ "Start when load drops")
complete -c make -s n -d (_ "Do not execute commands")
complete -c make -s o -r -d (_ "Ignore specified file")
complete -c make -s p -d (_ "Print database")
complete -c make -s q -d (_ "Question mode")
complete -c make -s r -d (_ "Eliminate implicit rules")
complete -c make -s s -d (_ "Quiet mode")
complete -c make -s S -d (_ "Don't continue after an error")
complete -c make -s t -d (_ "Touch files, don't run commands")
complete -c make -s v -d (_ "Display version and exit")
complete -c make -s w -d (_ "Print working directory")
complete -c make -s W -r -d (_ "Pretend file is modified")

