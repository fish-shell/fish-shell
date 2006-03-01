#
# These completions are uncomplete
#
complete -c emacs -s q -d (N_ "Do not load init files")
complete -c emacs -s u -d (N_ "Load users init file") -xa "(__fish_complete_users)"
complete -c emacs -s t -d (N_ "Use file as terminal") -r
complete -c emacs -s f -d (N_ "Execute Lisp function") -x
complete -c emacs -s l -d (N_ "Load Lisp code from file") -r
complete -c emacs -o nw -d (N_ "Do not use X interface")
complete -uc emacs -s d -o display -d (N_ "Create window on the specified display") -x
