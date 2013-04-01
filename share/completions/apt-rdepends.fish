#apt-rdepends
complete -c apt-rdepends -l help --description "Display help and exit"
complete -f -c apt-rdepends -s b -l build-depends --description "Show build dependencies"
complete -f -c apt-rdepends -s d -l dotty --description "Generate a dotty graph"
complete -f -c apt-rdepends -s p -l print-state --description "Show state of dependencies"
complete -f -c apt-rdepends -s r -l reverse --description "List packages depending on"
complete -r -f -c apt-rdepends -s f -l follow --description "Comma-separated list of dependency types to follow recursively"
complete -r -f -c apt-rdepends -s s -l show --description "Comma-separated list of dependency types to show"
complete -r -f -c apt-rdepends -l state-follow --description "Comma-separated list of package installation states to follow recursively"
complete -r -f -c apt-rdepends -l state-show --description "Comma-separated list of package installation states to show"
complete -f -c apt-rdepends -l man --description "Display man page"
complete -f -c apt-rdepends -l version --description "Display version and exit"

