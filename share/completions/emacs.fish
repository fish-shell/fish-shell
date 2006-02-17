#
# These completions are uncomplete
#
complete -c emacs -s q -d (_ "Do not load init files")
complete -c emacs -s u -d (_ "Load users init file") -xa "(__fish_complete_users)"
complete -c emacs -s t -d (_ "Use file as terminal") -r
complete -c emacs -s f -d (_ "Execute Lisp function") -x
complete -c emacs -s l -d (_ "Load Lisp code from file") -r
complete -c emacs -o nw -d (_ "Do not use X interface")
complete -uc emacs -s d -o display -d (_ "Create window on the specified display") -x
