complete -c history -r -l prefix --description "Match history items that start with the given prefix"
complete -c history -r -l contains --description "Match history items that contain the given string"
complete -c history -l search --description "Print matching history items, which is the default behavior"
complete -c history -l delete --description "Interactively delete matching history items"
complete -c history -l clear --description "Clear your entire history"
complete -c history -l merge --description "Incorporate history changes from other sessions"
# --save is not completed; it is for internal use
