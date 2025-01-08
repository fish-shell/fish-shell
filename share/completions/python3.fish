# This function adjusts for options with arguments (-X, -W, etc.), ensures completions stop when a script/module is set (-c, -m, file, -)
function __fish_python_no_arg
    set -l num 1
    set -l tokens (commandline -pxc)
    set -l has_arg_list -X -W --check-hash-based-pycs
    if contains -- - $tokens
        set num (math $num - 1)
    end
    for has_arg in $has_arg_list
        if contains -- $has_arg $tokens
            set num (math $num + 1)
        end
    end
    if test (__fish_number_of_cmd_args_wo_opts) -gt $num
        return 1
    end
    return 0
end

complete -c python3 -n __fish_python_no_arg -s B -d 'Don\'t write .py[co] files on import'
complete -c python3 -n __fish_python_no_arg -s c -x -d "Execute argument as command"
complete -c python3 -n __fish_python_no_arg -s d -d "Debug on"
complete -c python3 -n __fish_python_no_arg -s E -d "Ignore environment variables"
complete -c python3 -n __fish_python_no_arg -s h -s '?' -l help -d "Display help and exit"
complete -c python3 -n __fish_python_no_arg -s i -d "Interactive mode after executing commands"
complete -c python3 -n __fish_python_no_arg -s O -d "Enable optimizations"
complete -c python3 -n __fish_python_no_arg -o OO -d "Remove doc-strings in addition to the -O optimizations"
complete -c python3 -n __fish_python_no_arg -s s -d 'Don\'t add user site directory to sys.path'
complete -c python3 -n __fish_python_no_arg -s S -d "Disable import of site module"
complete -c python3 -n __fish_python_no_arg -s u -d "Unbuffered input and output"
complete -c python3 -n __fish_python_no_arg -s v -d "Verbose mode"
complete -c python3 -n __fish_python_no_arg -s V -l version -d "Display version and exit"
complete -c python3 -n __fish_python_no_arg -s W -x -d "Warning control" -a "ignore default all module once error"
complete -c python3 -n __fish_python_no_arg -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python3 -n __fish_python_no_arg -k -fa "(__fish_complete_suffix .py)"
complete -c python3 -n __fish_python_no_arg -fa - -d 'Read program from stdin'
complete -c python3 -n __fish_python_no_arg -s q -d 'Don\'t print version and copyright messages on interactive startup'
complete -c python3 -n __fish_python_no_arg -s X -x -d 'Set implementation-specific option' -a 'faulthandler showrefcount tracemalloc showalloccount importtime dev utf8 pycache_prefix=PATH:'
complete -c python3 -n __fish_python_no_arg -s b -d 'Issue warnings for possible misuse of `bytes` with `str`'
complete -c python3 -n __fish_python_no_arg -o bb -d 'Issue errors for possible misuse of `bytes` with `str`'
complete -c python3 -n __fish_python_no_arg -s m -d 'Run library module as a script (terminates option list)' -xa '(python3 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
complete -c python3 -n __fish_python_no_arg -l check-hash-based-pycs -d 'Set pyc hash check mode' -xa "default always never"
complete -c python3 -n __fish_python_no_arg -s I -d 'Run in isolated mode'
