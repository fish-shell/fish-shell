
# magic completion safety check (do not remove this comment)
if not type -q functions
    exit
end
complete -c functions -s e -l erase -d "Erase function" -x -a "(functions -n)"
complete -c functions -xa "(functions -na)" -d "Function"
complete -c functions -s a -l all -d "Show hidden functions"
complete -c functions -s h -l help -d "Display help and exit"
complete -c functions -s d -l description -d "Set function description" -x
complete -c functions -s q -l query -d "Test if function is defined"
complete -c functions -s n -l names -d "List the names of the functions, but not their definition"
complete -c functions -s c -l copy -d "Copy the specified function to the specified new name"
complete -c functions -s D -l details -d "Display information about the function"
complete -c functions -s v -l verbose -d "Print more output"
