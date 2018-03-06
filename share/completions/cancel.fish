
# magic completion safety check (do not remove this comment)
if not type -q cancel
    exit
end
__fish_complete_lpr cancel
complete -c cancel -s u -d 'Cancel jobs owned by username' -xa '(__fish_complete_users)'

