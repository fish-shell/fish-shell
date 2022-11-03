# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c fg -x -a "(__fish_complete_job_pids)"
complete -c fg -s h -l help -d 'Display help and exit'
