# Completion for builtin path
# This follows a strict command-then-options approach, so we can just test the number of tokens
complete -f -c path -n "test (count (commandline -opc)) -le 2" -s h -l help -d "Display help and exit"
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a basename -d 'Give basename for given paths'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a dirname -d 'Give dirname for given paths'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a extension -d 'Give extension for given paths'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a change-extension -d 'Change extension for given paths'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a normalize -d 'Normalize given paths (remove ./, resolve ../ against other components..)'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a resolve -d 'Normalize given paths and resolve symlinks'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a filter -d 'Print paths that match a filter'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a is -d 'Return true if any path matched a filter'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a match -d 'Match paths against a glob'
complete -f -c path -n "test (count (commandline -opc)) -lt 2" -a expand -d 'Expand globs'
complete -f -c path -n "test (count (commandline -opc)) -ge 2" -s q -l quiet -d "Only return status, no output"
complete -f -c path -n "test (count (commandline -opc)) -ge 2" -s z -l null-in -d "Handle NULL-delimited input"
complete -f -c path -n "test (count (commandline -opc)) -ge 2" -s Z -l null-out -d "Print NULL-delimited output"
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match" -s v -l invert -d "Invert meaning of filters"
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s t -l type -d "Filter by type" -x -a '(__fish_append , file link dir block char fifo socket)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s f -d "Filter files" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s d -d "Filter directories" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s l -d "Filter symlinks" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s p -l perm -d "Filter by permission" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s r -d "Filter readable paths" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s w -d "Filter writable paths" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
complete -f -c path -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] filter is match expand" -s x -d "Filter executale paths" -x -a '(__fish_append , read write exec suid sgid sticky user group)'
# Turn on file completions again.
# match takes a glob as first arg, expand takes only globs.
# We still want files completed then!
complete -F -c path -n "test (count (commandline -opc)) -ge 2"
