# Completion for builtin path
# This follows a strict command-then-options approach, so we can just test the number of tokens
complete -f -c path -n "test (count (commandline -xpc)) -le 2" -s h -l help -d "Display help and exit"
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a basename -d 'Give basename for given paths'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a dirname -d 'Give dirname for given paths'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a extension -d 'Give extension for given paths'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a change-extension -d 'Change extension for given paths'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a mtime -d 'Show modification time'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a normalize -d 'Normalize given paths (remove ./, resolve ../ against other components..)'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a resolve -d 'Normalize given paths and resolve symlinks'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a filter -d 'Print paths that match a filter'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a is -d 'Return true if any path matched a filter'
complete -f -c path -n "test (count (commandline -xpc)) -lt 2" -a sort -d 'Sort paths'
complete -f -c path -n "test (count (commandline -xpc)) -ge 2" -s q -l quiet -d "Only return status, no output"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2" -s z -l null-in -d "Handle NULL-delimited input"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2" -s Z -l null-out -d "Print NULL-delimited output"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] basename" -s E -l no-extension -d "Remove the extension"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s v -l invert -d "Invert meaning of filters"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s t -l type -d "Filter by type" -x -a '(__fish_append , file link dir block char fifo socket)'
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s f -d "Filter files"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s d -d "Filter directories"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s l -d "Filter symlinks"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s p -l perm -d "Filter by permission" -x -a '(__fish_append , read write exec suid sgid user group)'
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s r -d "Filter readable paths"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s w -d "Filter writable paths"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] filter is" -s x -d "Filter executable paths"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] mtime" -s R -l relative -d "Show seconds since the modification time"
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] sort" \
    -l key -x -a 'basename\t"Sort only by basename" dirname\t"Sort only by dirname" path\t"Sort by full path"'
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] sort" -s u -l unique -d 'Only leave the first of each run with the same key'
complete -f -c path -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] sort" -s r -l reverse -d 'Reverse the order'

# Turn on file completions again.
# match takes a glob as first arg, expand takes only globs.
# We still want files completed then!
complete -F -c path -n "test (count (commandline -xpc)) -ge 2"
