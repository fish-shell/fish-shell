
# magic completion safety check (do not remove this comment)
if not type -q cupsreject
    exit
end
__fish_complete_lpr cupsreject
complete -c cupsreject -s r -d 'Reject reason' -x

