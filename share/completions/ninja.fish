complete -c ninja -f -a '(__fish_print_ninja_targets)' -d target
complete -x -c ninja -s t -x -a "(__fish_print_ninja_tools)" -d subtool
complete -x -c ninja -s C -x -a "(__fish_complete_directories (commandline -ct))" -d "change to specified directory"
complete -c ninja -s f -x -a "(__fish_complete_suffix .ninja)" -d "specify build file [default=build.ninja]"
complete -f -c ninja -s n -d "dry run"
complete -f -c ninja -s v -d "show all command lines while building"
complete -f -c ninja -s j -d "number of jobs to run in parallel [default derived from CPUs]"
complete -f -c ninja -s l -d "do not start if load average > N"
complete -f -c ninja -s k -d "keep going until N jobs fail [default=1]"
complete -f -c ninja -s h -d "show help"
complete -f -c ninja -s d -d "enable debugging, specify debug mode"
complete -f -c ninja -s w -d "adjust warnings, specify flags"
complete -f -c ninja -l version -d "print ninja version"
