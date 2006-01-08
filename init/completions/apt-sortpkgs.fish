#apt-sortpkgs
complete -c apt-sortpkgs -s h -l help -d (_ "Apt-sortpkgs command help")
complete -f -c apt-sortpkgs -s s -l source -d (_ "Use source index field")
complete -f -c apt-sortpkgs -s v -l version -d (_ "Show version")
complete -r -c apt-sortpkgs -s c -l conf-file -d (_ "Specify conffile")
complete -r -f -c apt-sortpkgs -s o -l option -d (_ "Set config options")

