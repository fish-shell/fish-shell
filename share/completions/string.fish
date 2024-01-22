# Completion for builtin string
# This follows a strict command-then-options approach, so we can just test the number of tokens
complete -f -c string
complete -f -c string -n "test (count (commandline -xpc)) -le 2" -s h -l help -d "Display help and exit"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "not contains -- (commandline -xpc)[2] escape collect pad" -s q -l quiet -d "Do not print output"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a lower
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a upper
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a length
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] length" -s V -l visible -d "Use the visible width, excluding escape sequences"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a sub
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] sub" -s s -l start -xa "(seq 1 10)" -d "Sepcify start index"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] sub" -s e -l end -xa "(seq 1 10)" -d "Sepcify end index"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] sub" -s l -l length -xa "(seq 1 10)" -d "Sepcify substring length"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a split
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a split0
complete -x -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr split0\?\$ -- (commandline -xpc)[2]' -s m -l max -a "(seq 1 10)" -d "Specify maximum number of splits"
complete -x -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr split0\?\$ -- (commandline -xpc)[2]' -s f -l fields -a "(seq 1 10)" -d "Specify fields"
complete -f -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr split0\?\$ -- (commandline -xpc)[2]' -s r -l right -d "Split right-to-left"
complete -f -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr split0\?\$ -- (commandline -xpc)[2]' -s n -l no-empty -d "Empty results excluded"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a collect
complete -f -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr collect\$ -- (commandline -xpc)[2]' -s N -l no-trim-newlines -d "Don't trim trailing newlines"
complete -f -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'string match -qr collect\$ -- (commandline -xpc)[2]' -s a -l allow-empty -d "Always print empty argument"

complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a join
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a join0
complete -f -c string -n 'test (count (commandline -xpc)) -ge 2' -n 'contains -- (commandline -xpc)[2] join' -s n -l no-empty -d "Empty strings excluded"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a trim
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] trim" -s l -l left -d "Trim only leading chars"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] trim" -s r -l right -d "Trim only trailing chars"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] trim" -s c -l chars -d "Specify the chars to trim (default: whitespace)" -x
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a escape
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a unescape
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] escape; or contains -- (commandline -xpc)[2] unescape" -s n -l no-quoted -d "Escape with \\ instead of quotes"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] escape; or contains -- (commandline -xpc)[2] unescape" -l style -d "Specify escaping style" -xa "
(printf '%s\t%s\n' script 'For use in scripts' \
 var 'For use as a variable name' \
 regex 'For string match -r, string replace -r' \
 url 'For use as a URL')"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a match
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match" -s n -l index -d "Report index, length of match"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match" -s v -l invert -d "Report only non-matches"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match" -s e -l entire -d "Show entire matching lines"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match" -s g -l groups-only -d "Only report capturing groups"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a replace
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] replace" -s f -l filter -d "Report only actual replacements"
# All replace options are also valid for match
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match replace" -s a -l all -d "Report every match"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match replace" -s i -l ignore-case -d "Case insensitive"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] match replace" -s r -l regex -d "Use regex instead of globs"

complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a repeat
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] repeat" -s n -l count -xa "(seq 1 10)" -d "Repetition count"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] repeat" -s m -l max -xa "(seq 1 10)" -d "Maximum number of printed chars"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] repeat" -s N -l no-newline -d "Remove newline"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a pad
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] pad" -s r -l right -d "Pad right instead of left"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] pad" -s c -l char -x -d "Character to use for padding"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] pad" -s w -l width -x -d "Integer width of the result, default is maximum width of inputs"
complete -f -c string -n "test (count (commandline -xpc)) -lt 2" -a shorten
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] shorten" -s l -l left -d "Remove from the left on"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] shorten" -s c -l char -x -d "Characters to use as ellipsis"
complete -x -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] shorten" -s m -l max -x -d "Integer width of the result, default is minimum non-zero width of inputs"
complete -f -c string -n "test (count (commandline -xpc)) -ge 2" -n "contains -- (commandline -xpc)[2] shorten" -s N -l no-newline -d "Only keep one line of each input"
