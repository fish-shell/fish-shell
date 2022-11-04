# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#atd
complete -f -c atd -s l -d "Limiting load factor"
complete -f -c atd -s b -d "Minimum interval in seconds"
complete -f -c atd -s d -d "Debug mode"
complete -f -c atd -s s -d "Process at queue only once"
