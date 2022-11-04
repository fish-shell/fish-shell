# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-config
complete -c apt-config -s h -l help -d "Display help and exit"
complete -c apt-config -a shell -d "Access config file from shell"
complete -f -c apt-config -a dump -d "Dump contents of config file"
complete -f -c apt-config -s v -l version -d "Display version and exit"
complete -r -c apt-config -s c -l config-file -d "Specify config file"
complete -x -c apt-config -s o -l option -d "Specify options"
