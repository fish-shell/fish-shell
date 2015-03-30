function __fish_complete_python -d 'Make completion for python' --argument-names cmd
    complete -c $cmd -s B --description 'Don\'t write .py[co] files on import'
    complete -c $cmd -s c -x --description "Execute argument as command"
    complete -c $cmd -s d --description "Debug on"
    complete -c $cmd -s E --description "Ignore environment variables"
    complete -c $cmd -s h -l help --description "Display help and exit"
    complete -c $cmd -s i --description "Interactive mode after executing commands"
    complete -c $cmd -s m -d 'Run library module as a script (terminates option list)' -xa "( find /usr/lib/(eval $cmd -V 2>| sed 's/ //; s/\..\$//; s/P/p/') \$PYTHONPATH -maxdepth 1 -name '*.py' -printf '%f\n' | sed 's/.py//')"
    complete -c $cmd -s O --description "Enable optimizations"
    complete -c $cmd -o OO --description "Remove doc-strings in addition to the -O optimizations"
    complete -c $cmd -s s --description 'Don\'t add user site directory to sys.path'
    complete -c $cmd -s S --description "Disable import of site module"
    complete -c $cmd -s u --description "Unbuffered input and output"
    complete -c $cmd -s v --description "Verbose mode"
    complete -c $cmd -s V --description "Display version and exit"
    complete -c $cmd -s W -x --description "Warning control" -a "ignore default all module once error"
    complete -c $cmd -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
    complete -c $cmd -a "(__fish_complete_suffix .py)"
    complete -c $cmd -a '-' -d 'Read program from stdin'

    switch (eval $cmd -V 2>&1 | head -n1 | sed 's/^.*[[:space:]]\([23]\)\..*/\1/')
        case 2
        complete -c $cmd -s 3 -d 'Warn about Python 3.x incompatibilities that 2to3 cannot trivially fix'
        complete -c $cmd -s t --description "Warn on mixed tabs and spaces"
        complete -c $cmd -s Q -x -a "old new warn warnall" --description "Division control"

        case 3
        complete -c $cmd -s q --description 'Don\'t print version and copyright messages on interactive startup'
        complete -c $cmd -s X -x -d 'Set implementation-specific option'
        complete -c $cmd -s b  -d 'Issue warnings about str(bytes_instance), str(bytearray_instance) and comparing bytes/bytearray with str'
        complete -c $cmd -o bb -d 'Issue errors'

    end
end
