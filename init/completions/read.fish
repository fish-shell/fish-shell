complete -c read -s h -l help -d "Display help and exit"
complete -c read -s p -l prompt -d "Set prompt command" -x
complete -c read -s x -l export -d "Export variable to subprocess"
complete -c read -s g -l global -d "Make variable scope global"
complete -c read -s l -l local -d "Make variable scope local"
complete -c read -s u -l unexport -d "Do not export variable to subprocess"

