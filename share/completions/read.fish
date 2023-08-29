# Completion for builtin read
complete -c read -s h -l help -d "Display help and exit"
complete -c read -s p -l prompt -d "Set prompt command" -x
complete -c read -s P -l prompt-str -d "Set prompt using provided string" -x
complete -c read -s x -l export -d "Export variable to subprocess"
complete -c read -s g -l global -d "Make variable scope global"
complete -c read -s l -l local -d "Make variable scope local"
complete -c read -s U -l universal -d "Share variable with all the users fish processes on the computer"
complete -c read -s u -l unexport -d "Do not export variable to subprocess"
complete -c read -s c -l command -d "Initial contents of read buffer when reading interactively" -r
complete -c read -s S -l shell -d "Read like the shell would"
complete -c read -s s -l silent -d "Mask input with ●"
complete -c read -s n -l nchars -d "Read the specified number of characters" -x
complete -c read -s a -l list -l array -d "Store the results as an array"
complete -c read -s R -l right-prompt -d "Set right-hand prompt command" -x
complete -c read -s z -l null -d "Use NUL character as line terminator"
complete -c read -s L -l line -d "Read each line into its own variable"
complete -c read -s d -l delimiter -d "Set string to use as delimiter" -x
complete -c read -s t -l tokenize -d "Use shell tokenization rules when splitting"
