# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c psub -s h -l help -d "Display help and exit"
complete -c psub -s f -l file -d "Communicate using a regular file, not a named pipe"
complete -c psub -s F -l fifo -d "Communicate using a named pipe"
complete -c psub -s s -l suffix -d "Append suffix to the filename"
