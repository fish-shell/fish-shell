# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

__fish_complete_lpr cancel
complete -c cancel -s u -d 'Cancel jobs owned by username' -xa '(__fish_complete_users)'
