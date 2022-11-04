# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-ftparchive
complete -c apt-ftparchive -s h -l help -d "Display help and exit"
complete -f -c apt-ftparchive -a packages -d "Generate package from source"
complete -f -c apt-ftparchive -a sources -d "Generate source index file"
complete -f -c apt-ftparchive -a contents -d "Generate contents file"
complete -f -c apt-ftparchive -a release -d "Generate release file"
complete -f -c apt-ftparchive -a clean -d "Remove records"
complete -f -c apt-ftparchive -l md5 -d "Generate MD5 sums"
complete -f -c apt-ftparchive -s d -l db -d "Use a binary db"
complete -f -c apt-ftparchive -s q -l quiet -d "Quiet mode"
complete -f -c apt-ftparchive -l delink -d "Perform delinking"
complete -f -c apt-ftparchive -l contents -d "Perform contents generation"
complete -c apt-ftparchive -s s -l source-override -d "Use source override"
complete -f -c apt-ftparchive -l readonly -d "Make caching db readonly"
complete -f -c apt-ftparchive -s v -l version -d "Display version and exit"
complete -r -c apt-ftparchive -s c -l config-file -d "Use config file"
complete -r -c apt-ftparchive -s o -l option -d "Set config options"
