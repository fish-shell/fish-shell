# Completions for zstdmt

complete -c zstdmt -s z -l compress -d "Compress (default)"
complete -c zstdmt -s d -l decompress -l uncompress -d Decompress -k -x -a "
(
    __fish_complete_suffix .zst
)
"

complete -c zstdmt -s t -l test -d "Test the integrity"
complete -r -c zstdmt -l train -d "Create a dictionary from specified file(s)"
complete -c zstdmt -s l -l list -d "List information about .zst file(s)"

for level in (seq 1 19)
    complete -c zstdmt -o $level -d "Set compression level"
end

complete -c zstdmt -l fast -d "Ultra-fast compression"
complete -c zstdmt -l ultra -d "Enable compression level beyond 19"
complete -c zstdmt -l long -d "Enable long distance matching with specified windowLog"
complete -c zstdmt -l single-thread -d "Single-thread mode"
complete -c zstdmt -l adapt -d "Dynamically adapt compression level to I/O conditions"
complete -c zstdmt -l stream-size -d "Optimize compression parameters for streaming input of specified bytes"
complete -c zstdmt -l size-hint -d "Optimize compression parameters for streaming input of approximately this size"
complete -c zstdmt -l rsyncable -d "Compress using a rsync-friendly method"
complete -r -c zstdmt -s D -d "Use specified file as dictionary"
complete -c zstdmt -l no-dictID -d "Do not write dictID into header"
complete -r -c zstdmt -s o -d "Specify file to save"
complete -c zstdmt -s f -l force -d "Overwrite without prompting"
complete -c zstdmt -s c -l stdout -d "Force write to stdout"
complete -c zstdmt -l sparse -d "Enable sparse mode"
complete -c zstdmt -l no-sparse -d "Disable sparse mode"
complete -c zstdmt -l rm -d "Remove input file(s) after de/compression"
complete -c zstdmt -s k -l keep -d "Keep input file(s) (default)"
complete -c zstdmt -s r -d "Recurse directories"
complete -c zstdmt -l filelist -d "Read a list of files"
complete -c zstdmt -l output-dir-flat -d "Specify a directory to output all files"

for format in zstd gzip xz lzma lz4
    complete -c zstdmt -l format="$format" -d "Specify the format to use for compression"
end

complete -c zstdmt -s h -l help -d "Show help"
complete -c zstdmt -s H -d "Show long help"
complete -c zstdmt -s V -l version -d "Show version"
complete -c zstdmt -s v -l verbose -d "Be verbose"
complete -c zstdmt -s q -l quiet -d "Suppress warnings"
complete -c zstdmt -l no-progress -d "Do not show the progress bar"
complete -c zstdmt -l no-check -d "Disable integrity check"
