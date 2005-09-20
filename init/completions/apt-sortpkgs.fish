#apt-sortpkgs
complete -c apt-sortpkgs -s h -l help -d "apt-sortpkgs command help"
complete -f -c apt-sortpkgs -s s -l source -d "use source index field"
complete -f -c apt-sortpkgs -s v -l version -d "show version"
complete -r -c apt-sortpkgs -s c -l conf-file -d "specify conffile"
complete -r -f -c apt-sortpkgs -s o -l option -d "set config options"

