complete -c functions -s e -l erase -d (N_ "Erase function") -x -a "(functions -n)"
complete -c functions -xa "(functions -na)" -d (N_ "Function")
complete -c functions -s a -l all -d (N_ "Show hidden functions")
complete -c functions -s h -l help -d (N_ "Display help and exit")
complete -c functions -s d -l description -d (N_ "Set function description") -x
complete -c functions -s q -l query -d (N_ "Test if function is defined")
