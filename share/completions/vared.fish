# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c vared -x -a "(set | string replace -r ' ' '\tVariable: ')"
complete -c vared -s h -l help -d "Display help and exit"
