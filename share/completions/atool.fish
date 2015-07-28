complete -c atool -r -d 'Manage file archives of various types'

##Commands:
complete -c atool -s l -l list -d 'list files in archive (als)'
complete -c atool -s x -l extract -d 'extract files from archive (aunpack)'
complete -c atool -s X -l extract-to -r -d 'extract archive to specified directory'
complete -c atool -s a -l add -d 'create archive (apack)'
complete -c atool -s c -l cat -d 'extract file to standard out (acat)'
complete -c atool -s d -l diff -d 'generate a diff between two archives (adiff)'
complete -c atool -s r -l repack -d 'repack archives to a different format (arepack)'
complete -c atool -l help -d 'display this help and exit'
complete -c atool -l version -d 'output version information and exit'

##Options:
complete -c atool -s e -l each -d 'execute command above for each file specified'
complete -c atool -s F -l format -d 'override archive format (see below)' -xa "(man atool | __fish_sgrep -E '^\s+(\S+)\s+\(\..+\)' | sed -r 's/\s+(\S+)\s+\((.+)\)/\1\t\2/')"
complete -c atool -s O -l format-option -x -d 'give specific options to the archiver'
complete -c atool -s D -l subdir -d 'always create subdirectory when extracting'
complete -c atool -s f -l force -d 'allow overwriting of local files'
complete -c atool -s q -l quiet -d 'decrease verbosity level by one'
complete -c atool -s v -l verbose -d 'increase verbosity level by one'
complete -c atool -s V -l verbosity -d 'specify verbosity (0, 1 or 2)' -xa '(seq 0 2)'
complete -c atool -s p -l page -d 'send output through pager'
complete -c atool -s 0 -l null -d 'filenames from standard in are null-byte separated'
complete -c atool -s E -l explain -d 'explain what is being done by atool'
complete -c atool -s S -l simulate -d 'simulation mode - no filesystem changes are made'
complete -c atool -s o -l option -x -d 'override a configuration option (KEY=VALUE)'
complete -c atool -l config -r -d 'load configuration defaults from file'
#man -P cat atool | grep '(default')'
