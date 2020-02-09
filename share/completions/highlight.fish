
complete -c highlight -s O -l out-format -d 'Output file in given format' -xa 'xterm256 latex tex rtf html xhtml ansi bbcode svg'
complete -c highlight -s t -l tab -d 'Specify tab length' -x
complete -c highlight -s i -l input -d 'Name of the input file' -r
complete -c highlight -s o -l output -d 'Name of the output file' -r
complete -c highlight -s d -l outdir -d 'Output directory' -r
complete -c highlight -s S -l syntax -d 'Set type of the source code' -xa "(highlight -p | sed -r 's/^(.*\S)\s+:\s*(\S+).*\$/\2\t\1/; /^\$/d')"
complete -c highlight -s s -l style -d 'Highlight style' -xa "(highlight --list-themes | sed '/^\$\| /d')"
