# Completions for unzstd

complete -c unzstd -k -x -a "
(
    __fish_complete_suffix .zst
)
"

complete -c unzstd -s t -l test -d "Test the integrity"
complete -r -c unzstd -l train -d "Create a dictionary from specified file(s)"
complete -c unzstd -s l -l list -d "List information about .zst file(s)"
complete -c unzstd -l long -d "Enable long distance matching with specified windowLog"
complete -c unzstd -l single-thread -d "Single-thread mode"
complete -r -c unzstd -s D -d "Use specified file as dictionary"
complete -r -c unzstd -s o -d "Specify file to save"
complete -c unzstd -s f -l force -d "Overwrite without prompting"
complete -c unzstd -s c -l stdout -d "Force write to stdout"
complete -c unzstd -l sparse -d "Enable sparse mode"
complete -c unzstd -l no-sparse -d "Disable sparse mode"
complete -c unzstd -l rm -d "Remove input file(s) after decompression"
complete -c unzstd -s k -l keep -d "Keep input file(s) (default)"
complete -c unzstd -s r -d "Recurse directories"
complete -c unzstd -l filelist -d "Read a list of files"
complete -c unzstd -l output-dir-flat -d "Specify a directory to output all files"

for format in zstd gzip xz lzma lz4
    complete -c unzstd -l format="$format" -d "Specify the format to use for decompression"
end

complete -c unzstd -s h -l help -d "Show help"
complete -c unzstd -s H -d "Show long help"
complete -c unzstd -s V -l version -d "Show version"
complete -c unzstd -s v -l verbose -d "Be verbose"
complete -c unzstd -s q -l quiet -d "Suppress warnings"
complete -c unzstd -l no-progress -d "Do not show the progress bar"
