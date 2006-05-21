#apt-show-versions
complete -c apt-show-source -s h -l help -d (N_ 'Display help and exit')
complete -r -c apt-show-versions -s p -l packages -a '(apt-cache pkgnames)' -d (N_ 'Print PKG versions')
complete -f -c apt-show-versions -s r -l regex -d (N_ 'Using regex')
complete -f -c apt-show-versions -s u -l upgradeable -d (N_ 'Print only upgradeable packages')
complete -f -c apt-show-versions -s a -l allversions -d (N_ 'Print all versions')
complete -f -c apt-show-versions -s b -l brief -d (N_ 'Print package name/distro')
complete -f -c apt-show-versions -s v -l verbose -d (N_ 'Print verbose info')
complete -f -c apt-show-versions -s i -l initialize -d (N_ 'Init or update cache only')
complete -r -c apt-show-versions -l status-file -d (N_ 'Read package from file')
complete -r -c apt-show-versions -o stf -d (N_ 'Read package from file')
complete -r -c apt-show-versions -l list-dir -a '(ls -Fp .|grep /\$) /var/lib/apt/lists /var/state/apt/lists' -d (N_ 'Specify APT list dir')
complete -r -c apt-show-versions -o ld -a '(ls -Fp .|grep /\$) /var/lib/apt/lists /var/state/apt/lists' -d (N_ 'Specify APT list dir')

