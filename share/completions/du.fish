
# magic completion safety check (do not remove this comment)
if not type -q du
    exit
end
complete -c du -s a -l all -d "Write size for all files"
complete -c du -l apparent-size -d "Print file size, not disk usage"
complete -c du -s B -l block-size -d "Block size"
complete -c du -s b -l bytes -d "Use 1B block size"
complete -c du -s c -l total -d "Produce grand total"
complete -c du -s D -l dereference-args -d "Dereference file symlinks"
complete -c du -s h -l human-readable -d "Human readable sizes"
complete -c du -s H -l si -d "Human readable sizes, powers of 1000"
complete -c du -s k -d "Use 1kB block size"
complete -c du -s l -l count-links -d "Count hard links multiple times"
complete -c du -s L -l dereference -d "Dereference all symlinks"
complete -c du -s S -l separate-dirs -d "Do not include subdirectory size"
complete -c du -s s -l summarize -d "Display only a total for each argument"
complete -c du -s x -l one-file-system -d "Skip other file systems"
complete -c du -s X -l exclude-from -r -d "Exclude files that match pattern in file"
complete -c du -l exclude -r -d "Exclude files that match pattern"
complete -c du -l max-depth -r -d "Recursion limit"
complete -c du -l help -d "Display help and exit"
complete -c du -l version -d "Display version and exit"

