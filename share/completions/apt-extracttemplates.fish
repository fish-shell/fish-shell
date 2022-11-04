# SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#apt-extracttemplates
complete -c apt-extracttemplates -s h -l help -d "Display help and exit"
complete -r -c apt-extracttemplates -s t -d "Set temp dir"
complete -r -c apt-extracttemplates -s c -d "Specify config file"
complete -r -c apt-extracttemplates -s o -d "Specify options"
