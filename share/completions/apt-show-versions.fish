#apt-show-versions
complete -c apt-show-source -s h -l help --description 'Display help and exit'
complete -r -c apt-show-versions -s p -l packages -a '(apt-cache pkgnames)' --description 'Print PKG versions'
complete -f -c apt-show-versions -s r -l regex --description 'Using regex'
complete -f -c apt-show-versions -s u -l upgradeable --description 'Print only upgradeable packages'
complete -f -c apt-show-versions -s a -l allversions --description 'Print all versions'
complete -f -c apt-show-versions -s b -l brief --description 'Print package name/distro'
complete -f -c apt-show-versions -s v -l verbose --description 'Print verbose info'
complete -f -c apt-show-versions -s i -l initialize --description 'Init or update cache only'
complete -r -c apt-show-versions -l status-file --description 'Read package from file'
complete -r -c apt-show-versions -o stf --description 'Read package from file'
complete -r -c apt-show-versions -l list-dir -a '(for i in */; echo $i; end) /var/lib/apt/lists /var/state/apt/lists' --description 'Specify APT list dir'
complete -r -c apt-show-versions -o ld -a '(for i in */; echo $i; end) /var/lib/apt/lists /var/state/apt/lists' --description 'Specify APT list dir'

