# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directories (commandline -ct))"
complete -c rmdir -l ignore-fail-on-non-empty -d "Ignore errors from non-empty directories"
complete -c rmdir -s p -l parents -d "Remove each component of path"
complete -c rmdir -s v -l verbose -d "Verbose mode"
complete -c rmdir -l help -d "Display help and exit"
complete -c rmdir -l version -d "Display version and exit"
