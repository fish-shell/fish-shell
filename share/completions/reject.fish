
# magic completion safety check (do not remove this comment)
if not type -q reject
    exit
end
__fish_complete_lpr reject
complete -c reject -s r -d 'Reject reason' -x

