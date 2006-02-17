#apt-show-versions
complete -c apt-show-source -s h -l help -d (_ 'Display help and exit')
complete -r -c apt-show-versions -s p -l packages -a '(apt-cache pkgnames)' -d (_ 'Print PKG versions')
complete -f -c apt-show-versions -s r -l regex -d (_ 'Using regex')
complete -f -c apt-show-versions -s u -l upgradeable -d (_ 'Print only upgradeable packages')
complete -f -c apt-show-versions -s a -l allversions -d (_ 'Print all versions')
complete -f -c apt-show-versions -s b -l brief -d (_ 'Print package name/distro')
complete -f -c apt-show-versions -s v -l verbose -d (_ 'Print verbose info')
complete -f -c apt-show-versions -s i -l initialize -d (_ 'Init or update cache only')
complete -r -c apt-show-versions -l status-file -d (_ 'Read package from file')
complete -r -c apt-show-versions -o stf -d (_ 'Read package from file')
complete -r -c apt-show-versions -l list-dir -a '(ls -Fp .|grep /$) /var/lib/apt/lists /var/state/apt/lists' -d (_ 'Specify APT list dir')
complete -r -c apt-show-versions -o ld -a '(ls -Fp .|grep /$) /var/lib/apt/lists /var/state/apt/lists' -d (_ 'Specify APT list dir')

