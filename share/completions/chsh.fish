#
# Completions for the chsh command
#

# This grep tries to match nonempty lines that do not start with hash
complete -c chsh -s s -l shell -x -a "( __fish_sgrep '^[^#]' /etc/shells)" -d "Specify your login shell"
complete -c chsh -s u -l help -d "Display help and exit"
complete -c chsh -s v -l version -d "Display version and exit"
complete -x -c chsh -a "(__fish_complete_users)"

