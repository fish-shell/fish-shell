# Completions for lz4cat

complete -c lz4cat -k -x -a "
(
    __fish_complete_suffix .lz4
)
"

complete -c lz4cat -s t -l test -d "Test the integrity"
complete -c lz4cat -l list -d "List information about .lz4 file(s)"
complete -r -c lz4cat -s D -d "Use specified file as dictionary"
complete -c lz4cat -l sparse -d "Enable sparse mode"
complete -c lz4cat -l no-sparse -d "Disable sparse mode"
complete -c lz4cat -s v -l verbose -d "Be verbose"
complete -c lz4cat -s q -l quiet -d "Suppress warnings"
complete -c lz4cat -s h -l help -d "Show help"
complete -c lz4cat -s H -d "Show long help"
complete -c lz4cat -s V -l version -d "Show version"
