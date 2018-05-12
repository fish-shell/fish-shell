# Completions for make
function __fish_complete_make_targets
    set directory (string replace -r '^make .*(-C ?|--directory(=| +))([^ ]*) .*$' '$3' -- $argv)
    if not test $status -eq 0 -a -d $directory
        set directory ''
    end
    set file (string replace -r '^make .*(-f ?|--file(=| +))([^ ]*) .*$' '$3' -- $argv)
    if not test $status -eq 0 -a -f $file
        set file ''
    end
    __fish_print_make_targets "$directory" "$file"
end

# This completion reenables file completion on
# assignments, so e.g. 'make foo FILES=<tab>' will receive standard
# filename completion.
complete -c make -n 'commandline -ct | string match -q "*=*"' -a "(__fish_complete_make_targets (commandline -c))" -d "Target"
complete -f -c make -n 'commandline -ct | not string match -q "*=*"' -a "(__fish_complete_make_targets (commandline -c))" -d "Target"
complete -c make -s f -d "Use file as makefile" -r
complete -x -c make -s C -l directory -x -a "(__fish_complete_directories (commandline -ct))" -d "Change directory"
complete -c make -s d -d "Debug mode"
complete -c make -s e -d "Environment before makefile"
complete -c make -s i -d "Ignore errors"
complete -x -c make -s I -d "Search directory for makefile" -a "(__fish_complete_directories (commandline -ct))"
complete -x -c make -s j -d "Number of concurrent jobs"
complete -c make -s k -d "Continue after an error"
complete -c make -s l -d "Start when load drops"
complete -c make -s n -d "Do not execute commands"
complete -c make -s o -r -d "Ignore specified file"
complete -c make -s p -d "Print database"
complete -c make -s q -d "Question mode"
complete -c make -s r -d "Eliminate implicit rules"
complete -c make -s s -d "Quiet mode"
complete -c make -s S -d "Don't continue after an error"
complete -c make -s t -d "Touch files, don't run commands"
complete -c make -s v -d "Display version and exit"
complete -c make -s w -d "Print working directory"
complete -c make -s W -r -d "Pretend file is modified"

