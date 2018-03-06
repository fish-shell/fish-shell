
# magic completion safety check (do not remove this comment)
if not type -q lpq
    exit
end

__fish_complete_lpr lpq
complete -c lpq -s l -d 'Requests a more verbose (long) reporting format'
