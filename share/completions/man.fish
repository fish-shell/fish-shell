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

complete -rc man -s C -d "Configuration file"
complete -xc man -s M -a "(__fish_complete_directories (commandline -ct))" -d Manpath
complete -rc man -s P -d Pager
complete -xc man -s S -d "Manual sections"
complete -c man -s a -d "Display all matches"
complete -c man -s c -d "Always reformat"
complete -c man -s d -d Debug
complete -c man -s D -d "Debug and run"
complete -c man -s f -d "Show whatis information"
complete -c man -s F -l preformat -d "Format only"
complete -c man -s h -d "Display help and exit"
complete -c man -s k -d "Show apropos information"
complete -c man -s K -d "Search in all man pages"
complete -xc man -s m -d "Set system"
complete -xc man -s p -d Preprocessors
complete -c man -s t -d "Format for printing"
complete -c man -s w -l path -d "Only print locations"
complete -c man -s W -d "Only print locations"

complete -c man -n 'string match -q -- "*/*" (commandline -t | string collect)' --force-files
if command -q man
    # We have a conditionally-defined man function,
    # so we need to check for existence here.
    if echo | MANPAGER=cat command man -l - &>/dev/null
        complete -c man -s l -l local-file -d "Local file" -r
    end
end
