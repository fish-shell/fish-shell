# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c acat -w atool
complete -c acat -a '(__fish_complete_atool_archive_contents)' -d 'Archive content'
