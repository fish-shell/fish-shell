# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-rdepends
complete -c apt-rdepends -l help -d "Display help and exit"
complete -f -c apt-rdepends -s b -l build-depends -d "Show build dependencies"
complete -f -c apt-rdepends -s d -l dotty -d "Generate a dotty graph"
complete -f -c apt-rdepends -s p -l print-state -d "Show state of dependencies"
complete -f -c apt-rdepends -s r -l reverse -d "List packages depending on"
complete -r -f -c apt-rdepends -s f -l follow -d "Comma-separated list of dependency types to follow recursively"
complete -r -f -c apt-rdepends -s s -l show -d "Comma-separated list of dependency types to show"
complete -r -f -c apt-rdepends -l state-follow -d "Comma-separated list of package installation states to follow recursively"
complete -r -f -c apt-rdepends -l state-show -d "Comma-separated list of package installation states to show"
complete -f -c apt-rdepends -l man -d "Display man page"
complete -f -c apt-rdepends -l version -d "Display version and exit"
