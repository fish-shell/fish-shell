# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c exit -s h -l help -d 'Display help and exit'
complete -c exit -x -a 0 -d "Quit with normal exit status"
complete -c exit -x -a 1 -d "Quit with abnormal exit status"
