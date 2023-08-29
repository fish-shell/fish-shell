complete -c hjson -n __fish_should_complete_switches -s j -l json -d "output formatted json"
complete -c hjson -n __fish_should_complete_switches -s c -l compact -d "output condensed json"

# these are "old-style" arguments so using `__fish_should_complete_switches` is needed
# to prevent completing them by default as regular arguments.
complete -c hjson -n __fish_should_complete_switches -a -sl -d "output the opening brace on the same line"
complete -c hjson -n __fish_should_complete_switches -a -quote -d "quote all strings"
complete -c hjson -n __fish_should_complete_switches -a "-quote=all" -d "quote all strings and keys"
complete -c hjson -n __fish_should_complete_switches -a -js -d "output in JSON-compatible format"
complete -c hjson -n __fish_should_complete_switches -a -rt -d "round trip comments"
complete -c hjson -n __fish_should_complete_switches -a -nocol -d "disable color output"
complete -c hjson -n __fish_should_complete_switches -a "-cond=" -d "set condense option [default 60]"

complete -c hjson -k -xa "(__fish_complete_suffix .hjson .json)"
