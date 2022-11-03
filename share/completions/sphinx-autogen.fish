# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c sphinx-autogen -s h -l help -d "Display usage summary"
complete -c sphinx-autogen -l version -d "Display Sphinx version"
complete -c sphinx-autogen -s o -d "Directory to place all output"
complete -c sphinx-autogen -s s -l suffix -d "Default suffix for files"
complete -c sphinx-autogen -s t -l templates -d "Custom template directory"
complete -c sphinx-autogen -s i -l imported-members -d "Document imported members"
