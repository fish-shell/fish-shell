complete -c tex -o help -d "Display help and exit"
complete -c tex -o version -d "Display version and exit"
complete -c tex -k -x -a "(
__fish_complete_suffix (commandline -ct) .tex '(La)TeX file'
)"

complete -c tex -o file-line-error -d "Show errors in style file:line"
complete -c tex -o no-file-line-error -d "Show errors not in style file:line"
complete -c tex -o halt-on-error -d "Stop processing at the first error"
complete -c tex -o interaction=batchmode -d "Set interaction mode"
complete -c tex -o interaction=nonstopmode -d "Set interaction mode"
complete -c tex -o interaction=scrollmode -d "Set interaction mode"
complete -c tex -o interaction=errorstopmode -d "Set interaction mode"
complete -c tex -o output-directory= -x -a "(__fish_complete_directories (commandline -ct))" -d "Output directory"
complete -c tex -o shell-escape -d "Enable \write18{SHELL COMMAND}"
complete -c tex -o no-shell-escape -d "Disable \write18{SHELL COMMAND}"
complete -c tex -o src-specials -d "Insert source specials into the DVI file"
