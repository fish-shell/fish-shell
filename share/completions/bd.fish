# -*-  mode:ghoti; tab-width:4  -*-
#
# ghoti-completion for ghoti-bd
# https://github.com/0rax/ghoti-bd
#

complete -c bd -f

complete -c bd -s c -d "Classic mode: goes back to the first directory named as the string"
complete -c bd -s s -d "Seems mode: goes back to the first directory containing string"
complete -c bd -s i -d "Case insensitive move (implies seems mode)"
complete -c bd -s h -x -d "Display help and exit"

function __ghoti_bd_complete_dirs
    string split / -- $PWD
end

complete -c bd -a '(__ghoti_bd_complete_dirs)'
