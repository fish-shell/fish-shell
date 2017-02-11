complete -c python2 -s B --description 'Don\'t write .py[co] files on import'
complete -c python2 -s c -x --description "Execute argument as command"
complete -c python2 -s d --description "Debug on"
complete -c python2 -s E --description "Ignore environment variables"
complete -c python2 -s h -l help --description "Display help and exit"
complete -c python2 -s i --description "Interactive mode after executing commands"
complete -c python2 -s O --description "Enable optimizations"
complete -c python2 -o OO --description "Remove doc-strings in addition to the -O optimizations"
complete -c python2 -s s --description 'Don\'t add user site directory to sys.path'
complete -c python2 -s S --description "Disable import of site module"
complete -c python2 -s u --description "Unbuffered input and output"
complete -c python2 -s v --description "Verbose mode"
complete -c python2 -s V --description "Display version and exit"
complete -c python2 -s W -x --description "Warning control" -a "ignore default all module once error"
complete -c python2 -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python2 -a "(__fish_complete_suffix .py)"
complete -c python2 -a '-' -d 'Read program from stdin'
complete -c python2 -s 3 -d 'Warn about Python 3.x incompatibilities that 2to3 cannot trivially fix'
complete -c python2 -s t --description "Warn on mixed tabs and spaces"
complete -c python2 -s Q -x -a "old new warn warnall" --description "Division control"
# Override this to use python2 instead of python
complete -c python2 -s m -d 'Run library module as a script (terminates option list)' -xa '(python2 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
