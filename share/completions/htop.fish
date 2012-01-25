# Completions for top
complete -c htop -s d --description "Update interval" -x
complete -c htop -s u --description "Monitor effective UID" -x -a "(__fish_complete_users)"
complete -c htop -l sort-key -d 'Sort column' -xa "(htop --sort-key '')"

