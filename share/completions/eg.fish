# Based on https://github.com/srsudar/eg/blob/master/eg/core.py#L75 source code

complete -c eg -s v -l version -d 'Show version and exit'
complete -c eg -s f -l config-file -d 'A path to a .egrc file'
complete -c eg -s e -l edit -d 'Edit custom examples for a specific command'
complete -c eg -l examples-dir -d 'A location to examples/ directory'
complete -c eg -s c -l custom-dir -d 'A path to a directory with user-defined examples'
complete -c eg -s p -l pager-cmd -d 'A pager'
complete -c eg -s l -l list -d 'Show all the programs with eg entries'
complete -c eg -l color -d 'Colorize output'
complete -c eg -l no-color -d 'Do not colorize output'
