function __fish_maybe_list_all_functions
    # if the current commandline token starts with an _, list all functions
    if string match -qr -- '^_' (commandline -ct)
        functions -an
    else
        functions -n
    end
end

complete -c functions -s e -l erase -d "Erase function" -x -a "(__fish_maybe_list_all_functions)"
complete -c functions -xa "(functions -na)" -d Function
complete -c functions -s a -l all -d "Show hidden functions"
complete -c functions -s h -l help -d "Display help and exit"
complete -c functions -s d -l description -d "Set function description" -x
complete -c functions -s q -l query -d "Test if function is defined"
complete -c functions -s n -l names -d "List the names of the functions, but not their definition"
complete -c functions -s c -l copy -d "Copy the specified function to the specified new name"
complete -c functions -s D -l details -d "Display information about the function"
complete -c functions -s v -l verbose -d "Print more output"
complete -c functions -s H -l handlers -d "Show event handlers"
complete -c functions -s t -l handlers-type -d "Show event handlers matching the given type" -x -a "signal variable exit job-id generic"
