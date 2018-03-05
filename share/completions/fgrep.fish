
# magic completion safety check (do not remove this comment)
if not type -q fgrep
    exit
end
complete -c grep -w fgrep
