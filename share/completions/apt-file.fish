# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-file
complete -c apt-file -s h -l help -d "Display help and exit"
complete -x -c apt-file -a update -d "Resync package contents from source"
complete -r -c apt-file -a search -d "Search package containing pattern"
complete -r -c apt-file -a list -d "List contents of a package matching pattern"
complete -x -c apt-file -a purge -d "Remove all gz files from cache"
complete -r -c apt-file -s c -l cache -d "Set cache dir"
complete -f -c apt-file -s v -l verbose -d "Verbose mode"
complete -c apt-file -s d -l cdrom-mount -d "Use cdrom-mount-point"
complete -f -c apt-file -s i -l ignore-case -d "Do not expand pattern"
complete -f -c apt-file -s x -l regexp -d "Pattern is regexp"
complete -f -c apt-file -s V -l version -d "Display version and exit"
complete -f -c apt-file -s a -l architecture -d "Set arch"
complete -r -c apt-file -s s -l sources-list -a '(set -l files /etc/apt/*; string replace /etc/apt/ "" -- $files)' -d "Set sources.list file"
complete -f -c apt-file -s l -l package-only -d "Only display package name"
complete -f -c apt-file -s F -l fixed-string -d "Do not expand pattern"
complete -f -c apt-file -s y -l dummy -d "Run in dummy mode"
