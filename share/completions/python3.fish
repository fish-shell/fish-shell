complete -c python3 -s B -d 'Don\'t write .py[co] files on import'
complete -c python3 -s c -x -d "Execute argument as command"
complete -c python3 -s d -d "Debug on"
complete -c python3 -s E -d "Ignore environment variables"
complete -c python3 -s h -s '?' -l help -d "Display help and exit"
complete -c python3 -s i -d "Interactive mode after executing commands"
complete -c python3 -s O -d "Enable optimizations"
complete -c python3 -o OO -d "Remove doc-strings in addition to the -O optimizations"
complete -c python3 -s s -d 'Don\'t add user site directory to sys.path'
complete -c python3 -s S -d "Disable import of site module"
complete -c python3 -s u -d "Unbuffered input and output"
complete -c python3 -s v -d "Verbose mode"
complete -c python3 -s V -l version -d "Display version and exit"
complete -c python3 -s W -x -d "Warning control" -a "ignore default all module once error"
complete -c python3 -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python3 -n "__fish_is_nth_token 1" -k -fa "(__fish_complete_suffix .py)"
complete -c python3 -f -n "__fish_is_nth_token 1" -a - -d 'Read program from stdin'
complete -c python3 -s q -d 'Don\'t print version and copyright messages on interactive startup'
complete -c python3 -s X -x -d 'Set implementation-specific option' -a 'faulthandler showrefcount tracemalloc showalloccount importtime dev utf8 pycache_prefex=PATH:'
complete -c python3 -s b -d 'Issue warnings for possible misuse of `bytes` with `str`'
complete -c python3 -o bb -d 'Issue errors for possible misuse of `bytes` with `str`'
complete -c python3 -s m -d 'Run library module as a script (terminates option list)' -xa '(python3 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
complete -c python3 -l check-hash-based-pycs -d 'Set pyc hash check mode' -xa "default always never"
complete -c python3 -s I -d 'Run in isolated mode'
