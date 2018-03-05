
# magic completion safety check (do not remove this comment)
if not type -q lualatex
    exit
end
complete -c tex -w lualatex

