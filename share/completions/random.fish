
# magic completion safety check (do not remove this comment)
if not type -q random
    exit
end

complete -c random -s h -l help -d "Display help and exit"
