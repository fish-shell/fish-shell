if __fish_supports_version expand
    complete -c expand -s i -l initial -d 'do not convert tabs after non blanks'
    complete -c expand -s t -l tabs -x -d 'have tabs NUMBER characters apart, not 8'
    complete -c expand -s t -l tabs -x -d 'use comma separated list of explicit tab positions'
    complete -c expand -l help -d 'display this help and exit'
    complete -c expand -l version -d 'output version information and exit'
else
    complete -c expand -s t -x -d 'have tabs this many characters apart, or use a comma separated list of tab positions'
end
