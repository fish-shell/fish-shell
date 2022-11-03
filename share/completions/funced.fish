# SPDX-FileCopyrightText: © 2011 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c funced -xa "(functions -na)" -d "Save function"
complete -c funced -s e -l editor -d 'Open function in external editor' -xa '(__fish_complete_command)'
complete -c funced -s i -l interactive -d 'Edit in interactive mode'
complete -c funced -s s -l save -d 'Autosave after successful edit'
