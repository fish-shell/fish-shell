#apt-rdepends
complete -c apt-rdepends -l help -d "apt-rdepends command help"
complete -f -c apt-rdepends -s b -l build-depends -d "show bulid deps"
complete -f -c apt-rdepends -s d -l dotty -d "generate a dotty graph"
complete -f -c apt-rdepends -s p -l print-state -d "show state of deps"
complete -f -c apt-rdepends -s r -l reverse -d "list pkgs depending on"
complete -r -f -c apt-rdepends -s f -l follow -d "only follow DEPENDS recursively"
complete -r -f -c apt-rdepends -s s -l show -d "only show DEPENDS"
complete -r -f -c apt-rdepends -l state-follow -d "only follow STATES recursively"
complete -r -f -c apt-rdepends -l state-show -d "only show STATES"
complete -f -c apt-rdepends -l man -d "display man page"
complete -f -c apt-rdepends -l version -d "print version"

