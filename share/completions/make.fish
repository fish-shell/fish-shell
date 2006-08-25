# Completions for make

function __fish_print_make_targets
	set files Makefile makefile GNUmakefile
	grep -h -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $files | cut -d ":" -f 1 | sed -e 's/^ *//;s/ *$//;s/  */\n/g' ^/dev/null
end

# This completion is a bit ugly.  It reenables file completion on
# assignments, so e.g. 'make foo FILES=<tab>' will recive standard
# filename completion.  Unfortunatly, this turns out to be a bit
# complicated to do.

set -l is_assignment "commandline -ct|grep '..*='" 
set -l complete_file_assignment '(commandline -ct)(complete --do-complete=this_command_does_not_exist\ (commandline -ct|sed -e \'s/.*=//\'))'
complete -c make --condition $is_assignment -a $complete_file_assignment

complete -x -c make -a "(__fish_print_make_targets)" -d (N_ "Target")
complete -r -c make -s f -d (N_ "Use file as makefile") -r
complete -x -c make -s C -x -a "(__fish_complete_directory (commandline -ct))" -d (N_ "Change directory")
complete -c make -s d -d (N_ "Debug mode")
complete -c make -s e -d (N_ "Environment before makefile")
complete -c make -s i -d (N_ "Ignore errors")
complete -x -c make -s I -d (N_ "Search directory for makefile") -a "(__fish_complete_directory (commandline -ct))"
complete -x -c make -s j -d (N_ "Number of concurrent jobs")
complete -c make -s k -d (N_ "Continue after an error")
complete -c make -s l -d (N_ "Start when load drops")
complete -c make -s n -d (N_ "Do not execute commands")
complete -c make -s o -r -d (N_ "Ignore specified file")
complete -c make -s p -d (N_ "Print database")
complete -c make -s q -d (N_ "Question mode")
complete -c make -s r -d (N_ "Eliminate implicit rules")
complete -c make -s s -d (N_ "Quiet mode")
complete -c make -s S -d (N_ "Don't continue after an error")
complete -c make -s t -d (N_ "Touch files, don't run commands")
complete -c make -s v -d (N_ "Display version and exit")
complete -c make -s w -d (N_ "Print working directory")
complete -c make -s W -r -d (N_ "Pretend file is modified")

