#apt-show-source
complete -c apt-show-source -s h -l help -d (_ 'Display help and exit')
complete -r -c apt-show-source -l status-file -d (_ 'Read package from file') -f
complete -r -c apt-show-source -o stf -d (_ 'Read package from file') -f
complete -r -c apt-show-source -l list-dir -a '(ls -Fp .|grep /$) /var/lib/apt/lists' -d (_ 'Specify APT list dir')
complete -r -c apt-show-source -o ld -a '(ls -Fp .|grep /$) /var/lib/apt/lists' -d (_ 'Specify APT list dir')
complete -r -c apt-show-source -s p -l package -a '(apt-cache pkgnames)' -d (_ 'List PKG info')
complete -f -c apt-show-source -l version-only -d (_ 'Display version and exit')
complete -f -c apt-show-source -s a -l all -d (_ 'Print all source packages with version')
complete -f -c apt-show-source -s v -l verbose -d (_ 'Verbose mode')
