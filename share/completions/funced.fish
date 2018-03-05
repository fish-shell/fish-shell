
# magic completion safety check (do not remove this comment)
if not type -q funced
    exit
end
complete -c funced -xa "(functions -na)" -d "Save function"
complete -c funced -s e -l editor -d 'Open function in external editor' -xa '(__fish_complete_command)'
complete -c funced -s i -l interactive -d 'Edit in interactive mode'
