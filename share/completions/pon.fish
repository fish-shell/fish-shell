
# magic completion safety check (do not remove this comment)
if not type -q pon
    exit
end
complete -c pon -d 'PPP connection' -xa '(__fish_complete_ppp_peer)'
