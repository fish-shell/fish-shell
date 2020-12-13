# Completions for lz4

complete -c lz4 -s z -l compress -d "Compress (default)"
complete -c lz4 -s d -l decompress -l uncompress -d Decompress -k -x -a "
(
    __fish_complete_suffix .lz4
)
"

complete -c lz4 -s t -l test -d "Test the integrity"
complete -c lz4 -l list -d "List information about .lz4 file(s)"

for level in (seq 1 12)
    complete -c lz4 -o $level -d "Set compression level"
end

complete -c lz4 -l fast -d "Ultra-fast compression"
complete -c lz4 -l best -d "Highest compression, same as -12"
complete -c lz4 -l favor-decSpeed -d "Generate file(s) optimized for decompression speed"
complete -r -c lz4 -s D -d "Use specified file as dictionary"
complete -c lz4 -s f -l force -d "Overwrite without prompting"
complete -c lz4 -s c -l stdout -l to-stdout -d "Force write to stdout"
complete -c lz4 -s m -l multiple -d "Multiple input files"
complete -c lz4 -s r -d "Recurse directories"
complete -c lz4 -o B4 -d "Set block size to 64KB"
complete -c lz4 -o B5 -d "Set block size to 256KB"
complete -c lz4 -o B6 -d "Set block size to 1MB"
complete -c lz4 -o B7 -d "Set block size to 4MB"
complete -c lz4 -o BI -d "Block independence (default)"
complete -c lz4 -o BD -d "Block dependency"
complete -c lz4 -l no-frame-crc -d "Disable frame checksum"
complete -c lz4 -l content-size -d "Header includes original size"
complete -c lz4 -l sparse -d "Enable sparse mode"
complete -c lz4 -l no-sparse -d "Disable sparse mode"
complete -c lz4 -s v -l verbose -d "Be verbose"
complete -c lz4 -s q -l quiet -d "Suppress warnings"
complete -c lz4 -s h -l help -d "Show help"
complete -c lz4 -s H -d "Show long help"
complete -c lz4 -s V -l version -d "Show version"
complete -c lz4 -s k -l keep -d "Keep input file(s) (default)"
complete -c lz4 -l rm -d "Remove input file(s) after de/compression"
