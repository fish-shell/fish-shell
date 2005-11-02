#apt-show-versions
complete -c apt-show-source -s h -l help -d 'apt-show-versions command help'
complete -r -c apt-show-versions -s p -l packages -a '(apt-cache pkgnames)' -d 'print PKG versions'
complete -f -c apt-show-versions -s r -l regex -d 'using regex'
complete -f -c apt-show-versions -s u -l upgradeable -d 'print only upgradeable pkgs'
complete -f -c apt-show-versions -s a -l allversions -d 'print all versions'
complete -f -c apt-show-versions -s b -l brief -d 'print pkg name/distro'
complete -f -c apt-show-versions -s v -l verbose -d 'print verbose info'
complete -f -c apt-show-versions -s i -l initialize -d 'init or update cache only'
complete -r -c apt-show-versions -l status-file -d 'read pkg from FILE'
complete -r -c apt-show-versions -o stf -d 'read pkg from FILE'
complete -r -c apt-show-versions -l list-dir -a '(ls -Fp .|grep /$) /var/lib/apt/lists /var/state/apt/lists' -d 'specify APT list dir'
complete -r -c apt-show-versions -o ld -a '(ls -Fp .|grep /$) /var/lib/apt/lists /var/state/apt/lists' -d 'specify APT list dir'

