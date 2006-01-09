#apt-rdepends
complete -c apt-rdepends -l help -d (_ "Display help and exit")
complete -f -c apt-rdepends -s b -l build-depends -d (_ "Show bulid deps")
complete -f -c apt-rdepends -s d -l dotty -d (_ "Generate a dotty graph")
complete -f -c apt-rdepends -s p -l print-state -d (_ "Show state of deps")
complete -f -c apt-rdepends -s r -l reverse -d (_ "List pkgs depending on")
complete -r -f -c apt-rdepends -s f -l follow -d (_ "Only follow DEPENDS recursively")
complete -r -f -c apt-rdepends -s s -l show -d (_ "Only show DEPENDS")
complete -r -f -c apt-rdepends -l state-follow -d (_ "Only follow STATES recursively")
complete -r -f -c apt-rdepends -l state-show -d (_ "Only show STATES")
complete -f -c apt-rdepends -l man -d (_ "Display man page")
complete -f -c apt-rdepends -l version -d (_ "Display version and exit")

