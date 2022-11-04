# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c renice -s p -d "Force following parameters to be process ID's (The default)"
complete -c renice -s g -d "Force following parameters to be interpreted as process group ID's"
complete -c renice -s u -d "Force following parameters to be interpreted as user names"
