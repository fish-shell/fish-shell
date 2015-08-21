#apt-show-source
complete -c apt-show-source -s h -l help --description 'Display help and exit'
complete -r -c apt-show-source -l status-file --description 'Read package from file' -f
complete -r -c apt-show-source -o stf --description 'Read package from file' -f
complete -r -c apt-show-source -l list-dir -a '(ls -Fp .| __fish_sgrep /\$) /var/lib/apt/lists' --description 'Specify APT list dir'
complete -r -c apt-show-source -o ld -a '(ls -Fp .| __fish_sgrep /\$) /var/lib/apt/lists' --description 'Specify APT list dir'
complete -r -c apt-show-source -s p -l package -a '(apt-cache pkgnames)' --description 'List PKG info'
complete -f -c apt-show-source -l version-only --description 'Display version and exit'
complete -f -c apt-show-source -s a -l all --description 'Print all source packages with version'
complete -f -c apt-show-source -s v -l verbose --description 'Verbose mode'
