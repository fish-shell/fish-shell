# SPDX-FileCopyrightText: © 2016 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c ranger -s d -l debug -d "Activate debug mode"
complete -c ranger -s c -l clean -d "Activate clean mode"
complete -c ranger -s r -l confdir -d "Change configuration directory"
complete -c ranger -l copy-config -x -d "Create copies of the default configuration" -a "all commands commands_full rc rifle scope"
complete -c ranger -l choosefile -r -d "Pick file with ranger"
complete -c ranger -l choosefiles -r -d "Pick multiple files with ranger"
complete -c ranger -l choosedir -r -d "Pick directory"
complete -c ranger -l selectfile -r -d "Open ranger with given file selected"
complete -c ranger -l list-unused-keys -d "List common keys which are not bound"
complete -c ranger -l list-tagged-files -f -d "List all tagged files with given tag (default *)"
complete -c ranger -l profile -d "Print statistics of CPU usage on exit"
complete -c ranger -l cmd -r -d "Execute command after configuration file read"
complete -c ranger -l version -d "Print version"
complete -c ranger -l help -s h -d "Print help"
