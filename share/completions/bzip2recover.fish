# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c bzip2recover -k -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bzip2recover -k -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"
