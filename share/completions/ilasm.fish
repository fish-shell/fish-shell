# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c ilasm -l help -d 'Show help and exit'
complete -c ilasm -l version -d 'Show version and exit'

complete -c ilasm -o 'output:' -r -d 'Specify the output file name'
complete -c ilasm -o exe -d 'Generate an exe'
complete -c ilasm -o scan_only -d 'Just scan the IL code and display tokens'
complete -c ilasm -o show_tokens -d 'Show tokens as they are parsed'
complete -c ilasm -o show_method_def -d 'Display method information when a method is defined'
complete -c ilasm -o show_method_ref -d 'Display method information when a method is referenced'
complete -c ilasm -o 'key:' -d 'Strongname (sign) the output assembly using the key pair'
