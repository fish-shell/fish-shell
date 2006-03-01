complete -c configure -s h -l help -x -a "short recursive" -d (N_ "Display help and exit")
complete -c configure -s V -l version -d (N_ "Display version and exit")
complete -c configure -s q -l quiet -d (N_ "Quiet mode")
complete -c configure -l cache-file -f -d (N_ "Cache test results in specified file")
complete -c configure -s C -l config-cache -d (N_ "Cache test results in file config.cache")
complete -c configure -s n -l no-create -d (N_ "Do not create output files")
complete -c configure -l srcdir -d (N_ "Set source directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l prefix -d (N_ "Architecture-independent install directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l exec-prefix -d (N_ "Architecture-dependent install directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l build -d (N_ "Configure for building on BUILD") -x
complete -c configure -l host -d (N_ "Cross-compile to build programs to run on HOST") -x
complete -c configure -l target -d (N_ "Configure for building compilers for TARGET") -x -u
