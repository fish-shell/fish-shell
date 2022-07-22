# Completion for grunt

# grunt-cli
# http://gruntjs.com/
#
# Copyright (c) 2012 Tyler Kellen, contributors
# Licensed under the MIT license.
# https://github.com/gruntjs/grunt/blob/master/LICENSE-MIT

function __grunt_print_tasks
    grunt --version --verbose 2>/dev/null | awk '/Available tasks: / {$1=$2=""; print $0}' | awk '{$1=$1}1' | string split ' '
end

complete -c grunt -fa '(__grunt_print_tasks)'
complete -c grunt -s h -l help -d "Display help text"
complete -c grunt -l base -d "Specify an alternate base path"
complete -c grunt -l no-color -d "Disable colored output"
complete -c grunt -l gruntfile -rd "Specify an alternate Gruntfile"
complete -c grunt -s d -l debug -d "Enable debugging mode for tasks that support it"
complete -c grunt -l stack -d "Print a stack trace when exiting with a warning or fatal error"
complete -c grunt -s f -l force -d "A way to force your way past warnings."
complete -c grunt -l tasks -rd "Additional directory paths to scan for task and "extra" files"
complete -c grunt -l npm -x -d "Npm-installed grunt plugins to scan for task and "extra" files"
complete -c grunt -l no-write -d "Disable writing files (dry run)"
complete -c grunt -s v -l verbose -d "Verbose mode. A lot more information output"
complete -c grunt -s V -l version -d "Print the grunt version. Combine with --verbose for more info"
complete -c grunt -l completion -d "Output shell auto-completion rules."
