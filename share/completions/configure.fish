complete -c configure -s h -l help -x -a "short recursive" -d "Display help and exit"
complete -c configure -s V -l version -d "Display version and exit"
complete -c configure -s q -l quiet -d "Quiet mode"
complete -c configure -l cache-file -f -d "Cache test results in specified file"
complete -c configure -s C -l config-cache -d "Cache test results in file config.cache"
complete -c configure -s n -l no-create -d "Do not create output files"
complete -c configure -l srcdir -d "Set source directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l prefix -d "Architecture-independent install directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l exec-prefix -d "Architecture-dependent install directory" -a "__fish_complete_directories (commandline -ct)" -x
complete -c configure -l build -d "Configure for building on BUILD" -x
complete -c configure -l host -d "Cross-compile to build programs to run on HOST" -x
complete -c configure -l target -d "Configure for building compilers for TARGET" -x

# use autoconf's --help to generate completions:
complete -c 'configure' -a '(for tok in (commandline -opc)
    if string match -q "*configure" -- $tok
        __fish_parse_configure $tok
        break
    end
end)'
