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

complete -c python -n __fish_python_no_arg -s B -d 'Don\'t write .py[co] files on import'
complete -c python -n __fish_python_no_arg -s c -x -d "Execute argument as command"
complete -c python -n __fish_python_no_arg -l check-hash-based-pycs -a "default always never" -d "Control validation behaviour of pyc files"
complete -c python -n __fish_python_no_arg -s d -d "Debug on"
complete -c python -n __fish_python_no_arg -s E -d "Ignore all PYTHON* env vars"
complete -c python -n __fish_python_no_arg -s h -s '?' -l help -d "Display help and exit"
complete -c python -n __fish_python_no_arg -s i -d "Interactive mode after executing commands"
complete -c python -n __fish_python_no_arg -s m -d 'Run library module as a script (terminates option list)' -xa '(python -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
complete -c python -n __fish_python_no_arg -s O -d "Enable optimizations"
complete -c python -n __fish_python_no_arg -o OO -d "Remove doc-strings in addition to the -O optimizations"
complete -c python -n __fish_python_no_arg -s s -d 'Don\'t add user site directory to sys.path'
complete -c python -n __fish_python_no_arg -s S -d "Disable import of site module"
complete -c python -n __fish_python_no_arg -s u -d "Unbuffered input and output"
complete -c python -n __fish_python_no_arg -s v -d "Verbose mode"
complete -c python -n __fish_python_no_arg -o vv -d "Even more verbose mode"
complete -c python -n __fish_python_no_arg -s V -l version -d "Display version and exit"
complete -c python -n __fish_python_no_arg -s W -x -d "Warning control" -a "ignore default all module once error"
complete -c python -n __fish_python_no_arg -s x -d 'Skip first line of source, allowing use of non-Unix forms of #!cmd'
complete -c python -n __fish_python_no_arg -f -k -a "(__fish_complete_suffix .py)"
complete -c python -n __fish_python_no_arg -f -a - -d 'Read program from stdin'

# Version-specific completions
# We have to detect this at runtime because pyenv etc can change
# what `python` refers to.
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s2"' -s 3 -d 'Warn about Python 3.x incompatibilities that 2to3 cannot trivially fix'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s2"' -s t -d "Warn on mixed tabs and spaces"
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s2"' -s Q -x -a "old new warn warnall" -d "Division control"
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -s q -d 'Don\'t print version and copyright messages on interactive startup'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -s X -x -d 'Set implementation-specific option'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -s b -d 'Warn when comparing bytes with str or int'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -o bb -d 'Error when comparing bytes with str or int'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -s R -d 'Turn on hash randomization'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -s I -d 'Run in isolated mode'
complete -c python -n 'python -V 2>&1 | string match -rq "^.*\s3" && __fish_python_no_arg' -o VV -d 'Print further version info'
