# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#
# Command specific completions for the awk command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c awk -s F -d 'Define the input field separator' -r
complete -c awk -s f -d 'Specify the pathname of the file progfile containing an awk program' -r
complete -c awk -s v -d 'Ensure that the assignment argument is in the same form as an assignment operand' -r
