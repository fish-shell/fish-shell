
# magic completion safety check (do not remove this comment)
if not type -q rgrep
    exit
end
complete -c grep -w rgrep
