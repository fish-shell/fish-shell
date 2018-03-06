
# magic completion safety check (do not remove this comment)
if not type -q adiff
    exit
end
complete -c adiff -w atool
