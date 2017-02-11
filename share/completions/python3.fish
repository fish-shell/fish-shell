complete -c python3 -s B --description 'Don\'t write .py[co] files on import'
complete -c python3 -s c -x --description "Execute argument as command"
complete -c python3 -s d --description "Debug on"
complete -c python3 -s E --description "Ignore environment variables"
complete -c python3 -s h -l help --description "Display help and exit"
complete -c python3 -s i --description "Interactive mode after executing commands"
complete -c python3 -s O --description "Enable optimizations"
complete -c python3 -o OO --description "Remove doc-strings in addition to the -O optimizations"
complete -c python3 -s s --description 'Don\'t add user site directory to sys.path'
complete -c python3 -s S --description "Disable import of site module"
complete -c python3 -s u --description "Unbuffered input and output"
complete -c python3 -s v --description "Verbose mode"
complete -c python3 -s V --description "Display version and exit"
complete -c python3 -s W -x --description "Warning control" -a "ignore default all module once error"
complete -c python3 -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python3 -a "(__fish_complete_suffix .py)"
complete -c python3 -a '-' -d 'Read program from stdin'
complete -c python3 -s q --description 'Don\'t print version and copyright messages on interactive startup'
complete -c python3 -s X -x -d 'Set implementation-specific option'
complete -c python3 -s b  -d 'Issue warnings about str(bytes_instance), str(bytearray_instance) and comparing bytes/bytearray with str'
complete -c python3 -o bb -d 'Issue errors'
complete -c python3 -s m -d 'Run library module as a script (terminates option list)' -xa '(python3 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'

