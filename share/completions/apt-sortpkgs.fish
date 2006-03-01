#apt-sortpkgs
complete -c apt-sortpkgs -s h -l help -d (N_ "Display help and exit")
complete -f -c apt-sortpkgs -s s -l source -d (N_ "Use source index field")
complete -f -c apt-sortpkgs -s v -l version -d (N_ "Display version and exit")
complete -r -c apt-sortpkgs -s c -l conf-file -d (N_ "Specify conffile")
complete -r -f -c apt-sortpkgs -s o -l option -d (N_ "Set config options")

