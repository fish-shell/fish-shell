#
# These completions are uncomplete
#
complete -c emacs -s q -d "Do not load init files"
complete -c emacs -s u -d "Load users init file" -xa "(__fish_complete_users)"
complete -c emacs -s t -d "Use file as terminal" -r
complete -c emacs -s f -d "Execute Lisp function" -x
complete -c emacs -s l -d "Load Lisp code from file" -r
complete -c emacs -o nw -d "Do not use X interface"
complete -uc emacs -s d -o display -d "Create window on the specified display" -x
