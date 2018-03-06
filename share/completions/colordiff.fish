
# magic completion safety check (do not remove this comment)
if not type -q colordiff
    exit
end
complete -c colordiff -w diff
