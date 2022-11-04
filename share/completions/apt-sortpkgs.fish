# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-sortpkgs
complete -c apt-sortpkgs -s h -l help -d "Display help and exit"
complete -f -c apt-sortpkgs -s s -l source -d "Use source index field"
complete -f -c apt-sortpkgs -s v -l version -d "Display version and exit"
complete -r -c apt-sortpkgs -s c -l conf-file -d "Specify conffile"
complete -r -f -c apt-sortpkgs -s o -l option -d "Set config options"
