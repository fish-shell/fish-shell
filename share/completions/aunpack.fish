# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c aunpack -w atool
complete -c aunpack -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
