complete -c configure -s h -l help -x -a "short recursive" -d (_ "Display help and exit")
complete -c configure -s V -l version -d (_ "Display version and exit")
complete -c configure -s q -l quiet -d (_ "Be less verbose")
complete -c configure -l cache-file -f -d (_ "Cache test results in specified file")
complete -c configure -s C -l config-cache -d (_ "Cache test results in file config.cache")
complete -c configure -s n -l no-create -d (_ "Do not create output files")
complete -c configure -l srcdir -d (_ "Set source directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l prefix -d (_ "Architecture-independent install directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l exec-prefix -d (_ "Architecture-dependent install directory") -a "__fish_complete_directory (commandline -ct)" -x
complete -c configure -l build -d (_ "configure for building on BUILD") -x
complete -c configure -l host -d (_ "cross-compile to build programs to run on HOST") -x
complete -c configure -l target -d (_ "configure for building compilers for TARGET") -x -u
