# test if we are using GNU du
set -l is_gnu
if du --version &>/dev/null
    set is_gnu --is-gnu
end

# shared switches - short on both, long on GNU
__fish_gnu_complete -c du -s a -l all -d "Write size for all files" $is_gnu
__fish_gnu_complete -c du -s c -l total -d "Produce grand total" $is_gnu
__fish_gnu_complete -c du -s d -l max-depth -x -d "Recursion limit" $is_gnu
__fish_gnu_complete -c du -s h -l human-readable -d "Human readable sizes" $is_gnu
__fish_gnu_complete -c du -s l -l count-links -d "Count hard links multiple times" $is_gnu
__fish_gnu_complete -c du -s L -l dereference -d "Follow all symlinks" $is_gnu
__fish_gnu_complete -c du -s P -l no-dereference -d "Do not follow symlinks (default)" $is_gnu
__fish_gnu_complete -c du -s s -l summarize -d "Display only a total for each argument" $is_gnu
__fish_gnu_complete -c du -s t -l threshold -x -d "Threshold size" $is_gnu
__fish_gnu_complete -c du -s x -l one-file-system -d "Skip other file systems" $is_gnu

# shared switches without long options or with shared long options (--si)
complete -c du -s H -d "Follow command-line symlinks only"
complete -c du -s k -d "Use 1KB block size"
complete -c du -s m -d "Use 1MB block size"
complete -c du -l si -d "Human readable sizes, powers of 1000"

# platform-specific switches
if test -n "$is_gnu"
    # GNU specific switches
    complete -c du -s 0 -l null -d "End each output line with NUL"
    complete -c du -l apparent-size -d "Print file size, not disk usage"
    complete -c du -s B -l block-size -x -a "K M G T P E KiB MiB GiB TiB PiB EiB KB MB GB TB PB EB" -d "Block size (e.g. 1M, KiB)"
    complete -c du -s b -l bytes -d "Use 1B block size"
    complete -c du -s D -l dereference-args -d "Follow command-line symlinks only"
    complete -c du -l files0-from -r -d "Summarize usage of NUL-terminated files in file"
    complete -c du -l inodes -d "List inode usage instead of block usage"
    complete -c du -s S -l separate-dirs -d "Do not include subdirectory size"
    complete -c du -l time -f -a "atime access use ctime status" -d "Show time as WORD instead of modification time"
    complete -c du -l time-style -x -a "full-iso long-iso iso" -d "Show times using style (or +FORMAT)"
    complete -c du -s X -l exclude-from -r -d "Exclude files that match pattern in file"
    complete -c du -l exclude -x -d "Exclude files that match pattern"
    complete -c du -l help -d "Display help and exit"
    complete -c du -l version -d "Display version and exit"
else
    # macOS/BSD specific features
    complete -c du -s A -d "Display apparent size"
    complete -c du -s g -d "Use 1GB block size"
    complete -c du -s I -x -d "Ignore files matching mask"
    complete -c du -s n -d "Ignore nodump files"
    complete -c du -s B -x -d "Block size in bytes"
    #complete -c du -s r -d "Generate read error messages. The flag exists for portability"
end
