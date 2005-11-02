#apt-show-source
complete -c apt-show-source -s h -l help -d 'apt-show-source command help'
complete -r -c apt-show-source -l status-file -d 'read pkg from FILE' -f
complete -r -c apt-show-source -o stf -d 'read pkg from FILE' -f
complete -r -c apt-show-source -l list-dir -a '(ls -Fp .|grep /$) /var/lib/apt/lists' -d 'specify APT list dir'
complete -r -c apt-show-source -o ld -a '(ls -Fp .|grep /$) /var/lib/apt/lists' -d 'specify APT list dir'
complete -r -c apt-show-source -s p -l package -a '(apt-cache pkgnames)' -d 'list PKG info'
complete -f -c apt-show-source -l version-only -d 'print version only'
complete -f -c apt-show-source -s a -l all -d 'print all src pkgs with version'
complete -f -c apt-show-source -s v -l verbose -d 'verbose message'
