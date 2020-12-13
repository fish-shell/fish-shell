# Completions for unlz4

complete -c unlz4 -k -x -a "
(
    __fish_complete_suffix .lz4
)
"

complete -c unlz4 -s t -l test -d "Test the integrity"
complete -c unlz4 -l list -d "List information about .lz4 file(s)"
complete -r -c unlz4 -s D -d "Use specified file as dictionary"
complete -c unlz4 -s f -l force -d "Overwrite without prompting"
complete -c unlz4 -s c -l stdout -l to-stdout -d "Force write to stdout"
complete -c unlz4 -s m -l multiple -d "Multiple input files"
complete -c unlz4 -l sparse -d "Enable sparse mode"
complete -c unlz4 -l no-sparse -d "Disable sparse mode"
complete -c unlz4 -s v -l verbose -d "Be verbose"
complete -c unlz4 -s q -l quiet -d "Suppress warnings"
complete -c unlz4 -s h -l help -d "Show help"
complete -c unlz4 -s H -d "Show long help"
complete -c unlz4 -s V -l version -d "Show version"
complete -c unlz4 -s k -l keep -d "Keep input file(s) (default)"
complete -c unlz4 -l rm -d "Remove input file(s) after decompression"
