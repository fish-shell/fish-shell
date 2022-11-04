# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c w -s h -d "Dont print header"
complete -c w -s u -d "Ignore username for time calculations"
complete -c w -s s -d "Short format"
complete -c w -s f -d "Toggle printing of remote hostname"
complete -c w -s V -d "Display version and exit"
complete -c w -x -a "(__fish_complete_users)" -d Username
