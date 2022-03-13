complete -c python2 -s B -d 'Don\'t write .py[co] files on import'
complete -c python2 -s c -x -d "Execute argument as command"
complete -c python2 -s d -d "Debug on"
complete -c python2 -s E -d "Ignore environment variables"
complete -c python2 -s h -l help -d "Display help and exit"
complete -c python2 -s i -d "Interactive mode after executing commands"
complete -c python2 -s O -d "Enable optimizations"
complete -c python2 -o OO -d "Remove doc-strings in addition to the -O optimizations"
complete -c python2 -s s -d 'Don\'t add user site directory to sys.path'
complete -c python2 -s S -d "Disable import of site module"
complete -c python2 -s u -d "Unbuffered input and output"
complete -c python2 -s v -d "Verbose mode"
complete -c python2 -s V -d "Display version and exit"
complete -c python2 -s W -x -d "Warning control" -a "ignore default all module once error"
complete -c python2 -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python2 -f -n "__fish_is_nth_token 1" -k -a "(__fish_complete_suffix .py)"
complete -c python2 -f -n "__fish_is_nth_token 1" -a - -d 'Read program from stdin'
complete -c python2 -s 3 -d 'Warn about Python 3.x incompatibilities that 2to3 cannot trivially fix'
complete -c python2 -s t -d "Warn on mixed tabs and spaces"
complete -c python2 -s Q -x -a "old new warn warnall" -d "Division control"
# Override this to use python2 instead of python
complete -c python2 -s m -d 'Run library module as a script (terminates option list)' -xa '(python2 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
