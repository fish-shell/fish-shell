complete -c configure -s h -l help -x -a "short recursive" --description "Display help and exit"
complete -c configure -s V -l version --description "Display version and exit"
complete -c configure -s q -l quiet --description "Quiet mode"
complete -c configure -l cache-file -f --description "Cache test results in specified file"
complete -c configure -s C -l config-cache --description "Cache test results in file config.cache"
complete -c configure -s n -l no-create --description "Do not create output files"
complete -c configure -l srcdir --description "Set source directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l prefix --description "Architecture-independent install directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l exec-prefix --description "Architecture-dependent install directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l build --description "Configure for building on BUILD" -x
complete -c configure -l host --description "Cross-compile to build programs to run on HOST" -x
complete -c configure -l target --description "Configure for building compilers for TARGET" -x

complete -c configure -u
