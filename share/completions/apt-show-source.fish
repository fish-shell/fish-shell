#apt-show-source
complete -c apt-show-source -s h -l help -d 'Display help and exit'
complete -r -c apt-show-source -l status-file -d 'Read package from file' -f
complete -r -c apt-show-source -o stf -d 'Read package from file' -f
complete -r -c apt-show-source -l list-dir -a '*/ /var/lib/apt/lists' -d 'Specify APT list dir'
complete -r -c apt-show-source -o ld -a '*/ /var/lib/apt/lists' -d 'Specify APT list dir'
complete -r -c apt-show-source -s p -l package -a '(apt-cache pkgnames)' -d 'List PKG info'
complete -f -c apt-show-source -l version-only -d 'Display version and exit'
complete -f -c apt-show-source -s a -l all -d 'Print all source packages with version'
complete -f -c apt-show-source -s v -l verbose -d 'Verbose mode'
