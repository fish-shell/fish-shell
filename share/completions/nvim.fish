
# magic completion safety check (do not remove this comment)
if not type -q nvim
    exit
end
complete -c nvim -w vim
