# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-key
complete -r -c apt-key -a add -d "Add a new key"
complete -f -c apt-key -a del -d "Remove a key"
complete -f -c apt-key -a list -d "List trusted keys"
