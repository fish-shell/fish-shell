# Completions for zstd

complete -c zstd -s z -l compress -d "Compress (default)"
complete -c zstd -s d -l decompress -l uncompress -d Decompress -k -x -a "
(
    __fish_complete_suffix .zst
)
"

complete -c zstd -s t -l test -d "Test the integrity"
complete -r -c zstd -l train -d "Create a dictionary from specified file(s)"
complete -c zstd -s l -l list -d "List information about .zst file(s)"

for level in (seq 1 19)
    complete -c zstd -o $level -d "Set compression level"
end

complete -c zstd -l fast -d "Ultra-fast compression"
complete -c zstd -l ultra -d "Enable compression level beyond 19"
complete -c zstd -l long -d "Enable long distance matching with specified windowLog"

complete -c zstd -o T0 -l threads=0 -d "Compress using as many threads as there are CPU cores on the system"

complete -c zstd -l single-thread -d "Single-thread mode"
complete -c zstd -l adapt -d "Dynamically adapt compression level to I/O conditions"
complete -c zstd -l stream-size -d "Optimize compression parameters for streaming input of specified bytes"
complete -c zstd -l size-hint -d "Optimize compression parameters for streaming input of approximately this size"
complete -c zstd -l rsyncable -d "Compress using a rsync-friendly method"
complete -r -c zstd -s D -d "Use specified file as dictionary"
complete -c zstd -l no-dictID -d "Do not write dictID into header"
complete -r -c zstd -s o -d "Specify file to save"
complete -c zstd -s f -l force -d "Overwrite without prompting"
complete -c zstd -s c -l stdout -d "Force write to stdout"
complete -c zstd -l sparse -d "Enable sparse mode"
complete -c zstd -l no-sparse -d "Disable sparse mode"
complete -c zstd -l rm -d "Remove input file(s) after de/compression"
complete -c zstd -s k -l keep -d "Keep input file(s) (default)"
complete -c zstd -s r -d "Recurse directories"
complete -c zstd -l filelist -d "Read a list of files"
complete -c zstd -l output-dir-flat -d "Specify a directory to output all files"

for format in zstd gzip xz lzma lz4
    complete -c zstd -l format="$format" -d "Specify the format to use for compression"
end

complete -c zstd -s h -l help -d "Show help"
complete -c zstd -s H -d "Show long help"
complete -c zstd -s V -l version -d "Show version"
complete -c zstd -s v -l verbose -d "Be verbose"
complete -c zstd -s q -l quiet -d "Suppress warnings"
complete -c zstd -l no-progress -d "Do not show the progress bar"
complete -c zstd -l no-check -d "Disable integrity check"
