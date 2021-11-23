complete -c funcsave -d "save function(s) to disk"
complete -c funcsave -xa "(functions -na)"
complete -c funcsave -s d -l directory -d "dir to save function(s) into" -a '$fish_function_path' -r
complete -c funcsave -s q -d "suppress output" -r
