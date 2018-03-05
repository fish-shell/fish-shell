
# magic completion safety check (do not remove this comment)
if not type -q accept
    exit
end
__fish_complete_lpr accept
complete -c accept -s r -d 'Accept reason' -x

