
# magic completion safety check (do not remove this comment)
if not type -q pgrep
    exit
end

__fish_complete_pgrep pgrep
complete -c pgrep -s d -r -d 'Output delimiter'
complete -c pgrep -s l    -d 'List the process name as well as the process ID'
