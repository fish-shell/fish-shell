function __fish_complete_atool --description 'Complete atool' --argument-names cmd
    complete -c $cmd -r -d 'Manage file archives of various types'

    ##Commands:
    complete -c $cmd -s l -l list               -d 'list files in archive (als)'
    complete -c $cmd -s x -l extract            -d 'extract files from archive (aunpack)'
    complete -c $cmd -s X -l extract-to -r      -d 'extract archive to specified directory'
    complete -c $cmd -s a -l add                -d 'create archive (apack)'
    complete -c $cmd -s c -l cat                -d 'extract file to standard out (acat)'
    complete -c $cmd -s d -l diff               -d 'generate a diff between two archives (adiff)'
    complete -c $cmd -s r -l repack             -d 'repack archives to a different format (arepack)'
    complete -c $cmd -l help                    -d 'display this help and exit'
    complete -c $cmd -l version                 -d 'output version information and exit'

    ##Options:
    complete -c $cmd -s e -l each               -d 'execute command above for each file specified'
    complete -c $cmd -s F -l format             -d 'override archive format (see below)' -xa "(man atool | sgrep -E '^\s+(\S+)\s+\(\..+\)' | sed -r 's/\s+(\S+)\s+\((.+)\)/\1\t\2/')"
    complete -c $cmd -s O -l format-option -x   -d 'give specific options to the archiver'
    complete -c $cmd -s D -l subdir             -d 'always create subdirectory when extracting'
    complete -c $cmd -s f -l force              -d 'allow overwriting of local files'
    complete -c $cmd -s q -l quiet              -d 'decrease verbosity level by one'
    complete -c $cmd -s v -l verbose            -d 'increase verbosity level by one'
    complete -c $cmd -s V -l verbosity          -d 'specify verbosity (0, 1 or 2)' -xa '(seq 0 2)'
    complete -c $cmd -s p -l page               -d 'send output through pager'
    complete -c $cmd -s 0 -l null               -d 'filenames from standard in are null-byte separated'
    complete -c $cmd -s E -l explain            -d 'explain what is being done by atool'
    complete -c $cmd -s S -l simulate           -d 'simulation mode - no filesystem changes are made'
    complete -c $cmd -s o -l option -x          -d 'override a configuration option (KEY=VALUE)'
    complete -c $cmd -l config -r               -d 'load configuration defaults from file'

    switch $cmd
        case als aunpack acat
        complete -c $cmd -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
    end
    #man -P cat atool | grep '(default')'

end
