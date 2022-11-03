# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c wait -xa '(__fish_complete_job_pids)'
complete -c wait -s n -l any -d 'Return as soon as the first job completes'
complete -c wait -s h -l help -d 'Display help and exit'
