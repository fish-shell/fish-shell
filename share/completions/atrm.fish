#atrm

# magic completion safety check (do not remove this comment)
if not type -q atrm
    exit
end
complete -f -c atrm -s V -d "Display version and exit"
