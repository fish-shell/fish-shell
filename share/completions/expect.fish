# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c expect -s c -r -d "execute command"
complete -c expect -s d -n "__fish_not_contain_opt -s d" -d "diagnostic output"
complete -c expect -s D -x -r -a "0 1" -n "__fish_not_contain_opt -s D" -d "debug value"
complete -c expect -s f -r -d "script path"
complete -c expect -s i -n "__fish_not_contain_opt -s i" -d "interactive mode"
complete -c expect -s v -n "__fish_not_contain_opt -s v" -d "print version"
complete -c expect -s N -n "__fish_not_contain_opt -s N" -d "skip global rc"
complete -c expect -s n -n "__fish_not_contain_opt -s n" -d "skip user rc"
complete -c expect -s b -n "__fish_not_contain_opt -s b" -d "read line by line"
