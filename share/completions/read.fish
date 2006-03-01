complete -c read -s h -l help -d (N_ "Display help and exit")
complete -c read -s p -l prompt -d (N_ "Set prompt command") -x
complete -c read -s x -l export -d (N_ "Export variable to subprocess")
complete -c read -s g -l global -d (N_ "Make variable scope global")
complete -c read -s l -l local -d (N_ "Make variable scope local")
complete -c read -s U -l universal -d (N_ "Make variable scope universal, i.e. share variable with all the users fish processes on this computer")
complete -c read -s u -l unexport -d (N_ "Do not export variable to subprocess")

