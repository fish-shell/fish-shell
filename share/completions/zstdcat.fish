# Completions for zstdcat

complete -c zstdcat -k -x -a "
(
    __fish_complete_suffix .zst
)
"

complete -c zstdcat -s t -l test -d "Test the integrity"
complete -c zstdcat -s l -l list -d "List information about .zst file(s)"
complete -c zstdcat -l long -d "Enable long distance matching with specified windowLog"
complete -c zstdcat -l single-thread -d "Single-thread mode"
complete -r -c zstdcat -s D -d "Use specified file as dictionary"
complete -c zstdcat -l sparse -d "Enable sparse mode"
complete -c zstdcat -l no-sparse -d "Disable sparse mode"
complete -c zstdcat -s r -d "Recurse directories"
complete -c zstdcat -l filelist -d "Read a list of files"

for format in zstd gzip xz lzma lz4
    complete -c zstdcat -l format="$format" -d "Specify the format to use for decompression"
end

complete -c zstdcat -s h -l help -d "Show help"
complete -c zstdcat -s H -d "Show long help"
complete -c zstdcat -s V -l version -d "Show version"
complete -c zstdcat -s v -l verbose -d "Be verbose"
complete -c zstdcat -s q -l quiet -d "Suppress warnings"
complete -c zstdcat -l no-progress -d "Do not show the progress bar"
