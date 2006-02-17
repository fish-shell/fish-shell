complete -c read -s h -l help -d (_ "Display help and exit")
complete -c read -s p -l prompt -d (_ "Set prompt command") -x
complete -c read -s x -l export -d (_ "Export variable to subprocess")
complete -c read -s g -l global -d (_ "Make variable scope global")
complete -c read -s l -l local -d (_ "Make variable scope local")
complete -c read -s U -l universal -d (_ "Make variable scope universal, i.e. share variable with all the users fish processes on this computer")
complete -c read -s u -l unexport -d (_ "Do not export variable to subprocess")

