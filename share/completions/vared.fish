
# magic completion safety check (do not remove this comment)
if not type -q vared
    exit
end
complete -c vared -x -a "(set|sed -e 's/ /'\t'Variable: /')"
complete -c vared -s h -l help -d "Display help and exit"

