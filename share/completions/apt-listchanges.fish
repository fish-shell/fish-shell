# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-listchanges
complete -c apt-listchanges -l help -d "Display help and exit"
complete -c apt-listchanges -l apt -d "Read filenames from pipe"
complete -f -c apt-listchanges -s v -l verbose -d "Verbose mode"
complete -f -c apt-listchanges -s f -l frontend -a "pager browser xterm-pager xterm-browser text mail none" -d "Select frontend interface"
complete -r -f -c apt-listchanges -l email-address -d "Specify email address"
complete -f -c apt-listchanges -s c -l confirm -d "Ask confirmation"
complete -f -c apt-listchanges -s a -l all -d "Display all changelogs"
complete -r -c apt-listchanges -l save-seen -d "Avoid changelogs from db in named file"
complete -r -f -c apt-listchanges -l which -a "news changelogs both" -d "Select display"
complete -f -c apt-listchanges -s h -l headers -d "Insert header"
complete -f -c apt-listchanges -l debug -d "Display debug info"
complete -r -c apt-listchanges -l profile -d "Select an option profile"
