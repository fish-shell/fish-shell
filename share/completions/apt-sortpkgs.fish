#apt-sortpkgs
complete -c apt-sortpkgs -s h -l help --description "Display help and exit"
complete -f -c apt-sortpkgs -s s -l source --description "Use source index field"
complete -f -c apt-sortpkgs -s v -l version --description "Display version and exit"
complete -r -c apt-sortpkgs -s c -l conf-file --description "Specify conffile"
complete -r -f -c apt-sortpkgs -s o -l option --description "Set config options"

