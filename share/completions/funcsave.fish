
# magic completion safety check (do not remove this comment)
if not type -q funcsave
    exit
end
complete -c funcsave -xa "(functions -na)" -d "Save function"
