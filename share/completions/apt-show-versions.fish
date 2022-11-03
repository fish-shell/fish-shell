# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-show-versions
complete -c apt-show-versions -s h -l help -d 'Display help and exit'
complete -r -c apt-show-versions -s p -l packages -a '(apt-cache pkgnames)' -d 'Print PKG versions'
complete -f -c apt-show-versions -s r -l regex -d 'Using regex'
complete -f -c apt-show-versions -s u -l upgradeable -d 'Print only upgradeable packages'
complete -f -c apt-show-versions -s a -l allversions -d 'Print all versions'
complete -f -c apt-show-versions -s b -l brief -d 'Print package name/distro'
complete -f -c apt-show-versions -s v -l verbose -d 'Print verbose info'
complete -f -c apt-show-versions -s i -l initialize -d 'Init or update cache only'
complete -r -c apt-show-versions -l status-file -d 'Read package from file'
complete -r -c apt-show-versions -o stf -d 'Read package from file'
complete -r -c apt-show-versions -l list-dir -a '(for i in */; echo $i; end) /var/lib/apt/lists /var/state/apt/lists' -d 'Specify APT list dir'
complete -r -c apt-show-versions -o ld -a '(for i in */; echo $i; end) /var/lib/apt/lists /var/state/apt/lists' -d 'Specify APT list dir'
