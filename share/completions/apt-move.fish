# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-move
complete -c apt-move -a get -d "Generate master file"
complete -c apt-move -a getlocal -d "Alias for 'get'"
complete -f -c apt-move -a move -d "Move packages to local tree"
complete -f -c apt-move -a delete -d "Delete obsolete package files"
complete -f -c apt-move -a packages -d "Build new local files"
complete -f -c apt-move -a fsck -d "Rebuild index files"
complete -f -c apt-move -a update -d "Move packages from cache to local mirror"
complete -f -c apt-move -a local -d "Alias for 'move delete packages'"
complete -f -c apt-move -a localupdate -d "Alias for 'update'"
complete -f -c apt-move -a mirror -d "Download package missing from mirror"
complete -f -c apt-move -a sync -d "Sync packages installed"
complete -f -c apt-move -a exclude -d 'test $LOCALDIR/.exclude file'
complete -c apt-move -a movefile -d "Move file specified on commandline"
complete -f -c apt-move -a listbin -d 'List packages that may serve as input to mirrorbin or mirrorsource'
complete -f -c apt-move -a mirrorbin -d "Fetch package from STDIN"
complete -f -c apt-move -a mirrorsrc -d "Fetch source package from STDIN"
complete -f -c apt-move -s a -d "Process all packages"
complete -c apt-move -s c -d "Use specific conffile"
complete -f -c apt-move -s f -d "Force deletion"
complete -f -c apt-move -s q -d "Suppresses normal output"
complete -f -c apt-move -s t -d "Test run"
