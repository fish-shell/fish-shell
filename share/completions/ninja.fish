complete -c ninja -f -a '(__fish_print_ninja_targets)' -d Target
complete -x -c ninja -s t -x -a "(__fish_print_ninja_tools)" -d Tools
complete -x -c ninja -s C -x -a "(__fish_complete_directories (commandline -ct))" -d "Change to DIR"
complete -c ninja -s f -x -d "specify build file"
complete -f -c ninja -s n -d "dry run"
complete -f -c ninja -s v -d "show all command lines while building"
complete -f -c ninja -s j -d "number of jobs"
complete -f -c ninja -s l -d "do not start if load > N"
complete -f -c ninja -s k -d "keep until N jobs fail"
complete -f -c ninja -s h -d "help"
