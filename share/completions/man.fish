
complete -xc man -a "(__fish_complete_man)"

complete -xc man -n 'not __fish_complete_man' -a 1 -d 'Program section'
complete -xc man -n 'not __fish_complete_man' -a 2 -d 'Syscall section'
complete -xc man -n 'not __fish_complete_man' -a 3 -d 'Library section'
complete -xc man -n 'not __fish_complete_man' -a 4 -d 'Device section'
complete -xc man -n 'not __fish_complete_man' -a 5 -d 'File format section'
complete -xc man -n 'not __fish_complete_man' -a 6 -d 'Games section'
complete -xc man -n 'not __fish_complete_man' -a 7 -d 'Misc section'
complete -xc man -n 'not __fish_complete_man' -a 8 -d 'Admin section'
complete -xc man -n 'not __fish_complete_man' -a 9 -d 'Kernel section'
complete -xc man -n 'not __fish_complete_man' -a tcl -d 'Tcl section'
complete -xc man -n 'not __fish_complete_man' -a n -d 'New section'
complete -xc man -n 'not __fish_complete_man' -a l -d 'Local section'
complete -xc man -n 'not __fish_complete_man' -a p
complete -xc man -n 'not __fish_complete_man' -a o -d 'Old section'

complete -rc man -s C --description "Configuration file"
complete -xc man -s M -a "(__fish_complete_directories (commandline -ct))" --description "Manpath"
complete -rc man -s P --description "Pager"
complete -xc man -s S --description "Manual sections"
complete -c man -s a --description "Display all matches"
complete -c man -s c --description "Always reformat"
complete -c man -s d --description "Debug"
complete -c man -s D --description "Debug and run"
complete -c man -s f --description "Show whatis information"
complete -c man -s F -l preformat --description "Format only"
complete -c man -s h --description "Display help and exit"
complete -c man -s k --description "Show apropos information"
complete -c man -s K --description "Search in all man pages"
complete -xc man -s m --description "Set system"
complete -xc man -s p --description "Preprocessors"
complete -c man -s t --description "Format for printing"
complete -c man -s w -l path --description "Only print locations"
complete -c man -s W --description "Only print locations"

