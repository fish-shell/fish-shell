#
# These completions are uncomplete
#

# Primarily complete text files
complete -c emacs -x -a "(__fish_complete_mime 'text/*')"

complete -c emacs -s q --description "Do not load init files"
complete -c emacs -s u --description "Load users init file" -xa "(__fish_complete_users)"
complete -c emacs -s t --description "Use file as terminal" -r
complete -c emacs -s f --description "Execute Lisp function" -x
complete -c emacs -s l --description "Load Lisp code from file" -r
complete -c emacs -o nw --description "Do not use X interface"
complete -uc emacs -s d -o display --description "Create window on the specified display" -x
