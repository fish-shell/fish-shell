# localization: skip(not-needed)
# Completion for https://codeberg.org/gpanders/ijq
# also at https://github.com/gpanders/ijq

complete ijq -o h -l help -o help -d 'print help'
complete ijq -o V -d 'output version'

complete ijq -o C -d 'color output'
complete ijq -o M -d 'no color, monochrome'
complete ijq -o R -d 'input raw string, not JSON'
complete ijq -o r -d 'output raw string unescaped'
complete ijq -o S -d 'sort keys in output'
complete ijq -o c -d 'no pretty-printing, compact'
complete ijq -o n -d 'use `null` as input'
complete ijq -o s -d 'turn all inputs into one array input'
complete ijq -o hide-input-pane -d 'hide input pane'

complete ijq -ro jqbin -d 'path to `jq` binary'
complete ijq -ro f -d 'read initial filter from file'
complete ijq -ro H -d 'history file'
