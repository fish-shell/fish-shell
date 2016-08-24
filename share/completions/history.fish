complete -c history -r -l prefix --description "Match items starting with prefix"
complete -c history -r -l contains --description "Match items containing string"
complete -c history -l search -s s --description "Prints commands from history matching query"
complete -c history -l delete -s d --description "Deletes commands from history matching query"
complete -c history -l clear --description "Clears history file"
complete -c history -l merge -s m --description "Incorporate history changes from other sessions"
complete -c history -l exact -s e --description "Match items in the history that are identicial"
complete -c history -l with-time -s t --description "Output with timestamps"

# --save is not completed; it is for internal use
