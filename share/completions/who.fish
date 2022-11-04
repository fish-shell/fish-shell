# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c who -s a -l all -d "Same as -b -d --login -p -r -t -T -u"
complete -c who -s b -l boot -d "Print time of last boot"
complete -c who -s d -l dead -d "Print dead processes"
complete -c who -s H -l heading -d "Print line of headings"
complete -c who -s i -l idle -d "Print idle time"
complete -c who -s l -l login -d "Print login process"
complete -c who -l lookup -d "Canonicalize hostnames via DNS"
complete -c who -s m -d "Print hostname and user for stdin"
complete -c who -s p -l process -d "Print active processes spawned by init"
complete -c who -s q -l count -d "Print all login names and number of users logged on"
complete -c who -s r -l runlevel -d "Print current runlevel"
complete -c who -s s -l short -d "Print name, line, and time"
complete -c who -s t -l time -d "Print last system clock change"
complete -c who -s T -l mesg -d "Print users message status as +, - or ?"
complete -c who -s w -l writable -d "Print users message status as +, - or ?"
complete -c who -l message -d "Print users message status as +, - or ?"
complete -c who -s u -l users -d "List users logged in"
complete -c who -l help -d "Display help and exit"
complete -c who -l version -d "Display version and exit"
