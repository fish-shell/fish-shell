
# magic completion safety check (do not remove this comment)
if not type -q cd
    exit
end
complete -c cd -a "(__fish_complete_cd)"
complete -c cd -s h -l help -d 'Display help and exit'
