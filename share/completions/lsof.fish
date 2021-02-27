complete -c lsof -s '?' -s h -d 'Print help and exit'
complete -c lsof -s a -d 'Causes list selections to be ANDed'
complete -c lsof -s A -r -d 'Use alternative name list file'
complete -c lsof -s b -d 'Avoid kernel functions that might block: lstat, readlink, stat'
complete -c lsof -s c -d 'Select the listing for processes, whose command begins with string (^ - negate)' -xa '(__fish_complete_proc)'
complete -c lsof -s C -d 'Do not report any pathname component from kernel\'s namecache'
complete -c lsof -s d -r -d 'specifies a list of file descriptors to exclude or include in the output listing'
complete -c lsof -s D -d 'use of device cache file' -xa '\?\t"report device cache file paths"
b\t"build the device cache file"
i\t"ignore the device cache file"
r\t"read the device cache file"
u\t"read and update the device cache file"'

complete -c lsof -s g -d 'select by group (^ - negates)' -xa '(__fish_complete_list , __fish_complete_groups)'
complete -c lsof -s l -d 'Convert UIDs to login names'
complete -c lsof -s p -d 'Select or exclude processes by pid' -xa '(__fish_complete_list , __fish_complete_pids)'
complete -c lsof -s R -d 'Print PPID'
complete -c lsof -s t -d 'Produce terse output (pids only, no header)'
complete -c lsof -s u -d 'select by user (^ - negates)' -xa '(__fish_complete_list , __fish_complete_users)'
