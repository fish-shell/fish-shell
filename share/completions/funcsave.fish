complete -c funcsave -xa "(functions -na)" -d "Save function"
complete -c funcsave -s d -l directory -d "Directory to save the functions" -a '$fish_function_path' -r
