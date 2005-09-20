complete -c functions -s e -l erase -d "Erase function" -x -a "(functions -n)"
complete -c functions -xa "(functions -n)" -d "Function"
complete -c functions -s a -l all -d "Show hidden functions"
complete -c functions -s h -l help -d "Display help and exit"
complete -c functions -s d -l description -d "Set function description" -x

