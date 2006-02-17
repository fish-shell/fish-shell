complete -c functions -s e -l erase -d (_ "Erase function") -x -a "(functions -n)"
complete -c functions -xa "(functions -na)" -d (_ "Function")
complete -c functions -s a -l all -d (_ "Show hidden functions")
complete -c functions -s h -l help -d (_ "Display help and exit")
complete -c functions -s d -l description -d (_ "Set function description") -x

