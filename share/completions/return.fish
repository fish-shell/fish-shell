# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c return -s h -l help -d "Display help and exit"
complete -c return -x -a 0 -d "Return from function with normal exit status"
complete -c return -x -a 1 -d "Return from function with abnormal exit status"
