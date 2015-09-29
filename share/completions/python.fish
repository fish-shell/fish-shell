complete -c python -s B --description 'Don\'t write .py[co] files on import'
complete -c python -s c -x --description "Execute argument as command"
complete -c python -s d --description "Debug on"
complete -c python -s E --description "Ignore environment variables"
complete -c python -s h -l help --description "Display help and exit"
complete -c python -s i --description "Interactive mode after executing commands"
complete -c python -s m -d 'Run library module as a script (terminates option list)' -xa '(python -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
complete -c python -s O --description "Enable optimizations"
complete -c python -o OO --description "Remove doc-strings in addition to the -O optimizations"
complete -c python -s s --description 'Don\'t add user site directory to sys.path'
complete -c python -s S --description "Disable import of site module"
complete -c python -s u --description "Unbuffered input and output"
complete -c python -s v --description "Verbose mode"
complete -c python -s V --description "Display version and exit"
complete -c python -s W -x --description "Warning control" -a "ignore default all module once error"
complete -c python -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python -a "(__fish_complete_suffix .py)"
complete -c python -a '-' -d 'Read program from stdin'

switch (python -V 2>&1 | string replace -r '^.*\s([23])..*' '$1')[1]
    case 2
        complete -c python -s 3 -d 'Warn about Python 3.x incompatibilities that 2to3 cannot trivially fix'
        complete -c python -s t --description "Warn on mixed tabs and spaces"
        complete -c python -s Q -x -a "old new warn warnall" --description "Division control"
    case 3
        complete -c python -s q --description 'Don\'t print version and copyright messages on interactive startup'
        complete -c python -s X -x -d 'Set implementation-specific option'
        complete -c python -s b  -d 'Issue warnings about str(bytes_instance), str(bytearray_instance) and comparing bytes/bytearray with str'
        complete -c python -o bb -d 'Issue errors'
end
