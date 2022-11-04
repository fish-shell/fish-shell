# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c bzcat -k -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bzcat -k -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"

complete -c bzcat -s s -l small -d "Reduce memory usage"
