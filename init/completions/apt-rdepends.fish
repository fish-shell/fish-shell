#apt-rdepends
complete -c apt-rdepends -l help -d (_ "Display help and exit")
complete -f -c apt-rdepends -s b -l build-depends -d (_ "Show bulid dependencies")
complete -f -c apt-rdepends -s d -l dotty -d (_ "Generate a dotty graph")
complete -f -c apt-rdepends -s p -l print-state -d (_ "Show state of dependencies")
complete -f -c apt-rdepends -s r -l reverse -d (_ "List packages depending on")
complete -r -f -c apt-rdepends -s f -l follow -d (_ "Comma-separated list of dependancy types to follow recursively")
complete -r -f -c apt-rdepends -s s -l show -d (_ "Comma-separated list of dependancy types to show")
complete -r -f -c apt-rdepends -l state-follow -d (_ "Comma-separated list of package installation states to follow recursively")
complete -r -f -c apt-rdepends -l state-show -d (_ "Comma-separated list of package installation states to show")
complete -f -c apt-rdepends -l man -d (_ "Display man page")
complete -f -c apt-rdepends -l version -d (_ "Display version and exit")

