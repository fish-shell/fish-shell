
# magic completion safety check (do not remove this comment)
if not type -q cupsaccept
    exit
end
__fish_complete_lpr cupsaccept
complete -c cupsaccept -s r -d 'Accept reason' -x

