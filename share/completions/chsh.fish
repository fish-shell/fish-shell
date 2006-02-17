#
# Completions for the chsh command
#

complete -c chsh -s s -l shell -x -a "(chsh -l)" -d "Specify your login shell"
complete -c chsh -s l -l list-shells -d "Display the list of shells listed in /etc/shells and exit"
complete -c chsh -s u -l help -d "Display help and exit"
complete -c chsh -s v -l version -d "Display version and exit"
complete -x -c chsh -a "(__fish_complete_users)"

