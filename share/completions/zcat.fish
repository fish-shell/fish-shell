# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c zcat -k -x -a "(
	__fish_complete_suffix .gz
	__fish_complete_suffix .tgz
)
"
complete -c zcat -s f -l force -d Overwrite
complete -c zcat -s h -l help -d "Display help and exit"
complete -c zcat -s L -l license -d "Print license"
complete -c zcat -s V -l version -d "Display version and exit"
