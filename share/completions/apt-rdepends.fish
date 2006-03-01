#apt-rdepends
complete -c apt-rdepends -l help -d (N_ "Display help and exit")
complete -f -c apt-rdepends -s b -l build-depends -d (N_ "Show build dependencies")
complete -f -c apt-rdepends -s d -l dotty -d (N_ "Generate a dotty graph")
complete -f -c apt-rdepends -s p -l print-state -d (N_ "Show state of dependencies")
complete -f -c apt-rdepends -s r -l reverse -d (N_ "List packages depending on")
complete -r -f -c apt-rdepends -s f -l follow -d (N_ "Comma-separated list of dependancy types to follow recursively")
complete -r -f -c apt-rdepends -s s -l show -d (N_ "Comma-separated list of dependancy types to show")
complete -r -f -c apt-rdepends -l state-follow -d (N_ "Comma-separated list of package installation states to follow recursively")
complete -r -f -c apt-rdepends -l state-show -d (N_ "Comma-separated list of package installation states to show")
complete -f -c apt-rdepends -l man -d (N_ "Display man page")
complete -f -c apt-rdepends -l version -d (N_ "Display version and exit")

